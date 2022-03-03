#include <unistd.h>
#include <xtrx_api.h>

#include "rf_helper.h"
#include "srsran/config.h"
#include "srsran/srsran.h"
#include "srsran/phy/rf/rf.h"
#include "rf_xtrx_imp.h"

#define PRINT_TX_STATS 0
#define PRINT_RX_STATS 0

cf_t zero_mem[64 * 1024];	/* dummy mem */

typedef struct
{
	struct xtrx_dev *dev;
	unsigned tx_inter;
	unsigned rx_decim;
	double rx_rate;	/* the actual SDR sampling rate */
	double tx_rate;	/* the actual SDR sampling rate */
	double rx_gain;
	double tx_gain;
	bool rx_stream_enabled;
	bool tx_stream_enabled;
	uint64_t next_tstamp;
	double current_srate_hz;
	srsran_rf_info_t info;
} rf_xtrx_handler_t;

uint32_t rf_xtrx_logging_level = XTRX_LOG_INFO;
static srsran_rf_error_handler_t xtrx_error_handler = NULL;
static void *xtrx_error_handler_arg = NULL;

void rf_xtrx_suppress_stdout(void *h_)
{
	xtrx_log_setlevel(XTRX_LOG_INFO,NULL);
	rf_xtrx_logging_level = XTRX_LOG_INFO;
}

void rf_xtrx_register_error_handler(void *h_,srsran_rf_error_handler_t new_handler,void *arg)
{
	xtrx_error_handler = new_handler;
	xtrx_error_handler_arg = arg;
}

const char* rf_xtrx_devname(void *h_)
{
	return(DEVNAME);
}

int rf_xtrx_start_tx_stream(void *h_)
{
	int res;
	rf_xtrx_handler_t *h = h_;
	xtrx_run_params_t params;

	xtrx_stop(h->dev,XTRX_TX);
	xtrx_set_antenna(h->dev,XTRX_TX_AUTO);

	xtrx_run_params_init(&params);

	params.dir = XTRX_TX;
	params.tx_repeat_buf = NULL;
	params.tx.paketsize = 0;
	params.tx.chs = XTRX_CH_AB;
	params.tx.wfmt = XTRX_WF_16;
	params.tx.hfmt = XTRX_IQ_FLOAT32;
	params.tx.flags = XTRX_RSP_SWAP_IQ|XTRX_RSP_SISO_MODE;
	res = xtrx_run_ex(h->dev,&params);
	if(res)
	{
		XTRX_RF_ERROR("xtrx_run_ex: can't start tx stream,err: %d\n",res);
		return(SRSRAN_ERROR);
	}

	h->tx_stream_enabled = true;
	return(SRSRAN_SUCCESS);
}

int rf_xtrx_start_rx_stream(void *h_,bool now)
{
	int res;
	rf_xtrx_handler_t *h = h_;
	xtrx_run_params_t params;

	xtrx_stop(h->dev,XTRX_RX);
	xtrx_set_antenna(h->dev,XTRX_RX_AUTO);

	xtrx_run_params_init(&params);

	params.dir = XTRX_RX;
	params.rx.paketsize = 0;
	params.rx.chs = XTRX_CH_AB;
	params.rx.wfmt = XTRX_WF_16;
	params.rx.hfmt = XTRX_IQ_FLOAT32;
	params.rx.scale = 32767;
	params.rx.flags = XTRX_RSP_SISO_MODE|XTRX_RSP_SCALE;
	params.rx_stream_start = 20000;
	res = xtrx_run_ex(h->dev,&params);
	if(res)
	{
		XTRX_RF_ERROR("xtrx_run_ex: can't start rx stream,err: %d\n",res);
		return(SRSRAN_ERROR);
	}

	h->rx_stream_enabled = true;
	return(SRSRAN_SUCCESS);
}

int rf_xtrx_stop_rx_stream(void *h_)
{
	rf_xtrx_handler_t *h = h_;

	xtrx_stop(h->dev,XTRX_RX);
	xtrx_stop(h->dev,XTRX_TX);
	h->rx_stream_enabled = false;
	h->tx_stream_enabled = false;
	return(0);
}

void rf_xtrx_flush_buffer(void *h_)
{
}

bool rf_xtrx_has_rssi(void *h)
{
	return(false);
}

float rf_xtrx_get_rssi(void *h)
{
	return(0);
}

int rf_xtrx_open_multi(char* args,void **h,uint32_t nof_channels)
{
	return(rf_xtrx_open(args,h));
}

int rf_xtrx_open(char *args,void **h_)
{
	unsigned flags;
	int res;
	unsigned int rx_decim = 0;
	unsigned int tx_inter = 0;
	int refclk = 26000000;
	char devs[500] = "/dev/xtrx0";
	char log_level[RF_PARAM_LEN] = "info";
	rf_xtrx_handler_t *h = malloc(sizeof(rf_xtrx_handler_t));
	if(!h)
	{
		perror("malloc");
		return(-1);
	}

	*h_ = h;

	parse_string(args,"log_level",0,log_level);
	if(strcmp(log_level,"info") == 0)
		rf_xtrx_logging_level = XTRX_LOG_INFO;
	else if(strcmp(log_level,"debug") == 0)
		rf_xtrx_logging_level = XTRX_LOG_DEBUG;
	else if(strcmp(log_level,"warn") == 0)
		rf_xtrx_logging_level = XTRX_LOG_WARNING;
	else if(strcmp(log_level,"error") == 0)
		rf_xtrx_logging_level = XTRX_LOG_ERROR;
	else
	{
		XTRX_RF_ERROR("log_level %s is undefined. Options: debug,info,warn and error\n",log_level);
		return(SRSRAN_ERROR);
	}

	flags = ((unsigned)rf_xtrx_logging_level) & XTRX_O_LOGLVL_MASK;
	parse_string(args,"devs",0,devs);
	res = xtrx_open(devs,flags,&h->dev);
	if(res)
	{
		XTRX_RF_ERROR("xtrx_open: can't open \"%s\" with \"%s\": %d error\n",devs,args,res);
		return(-1);
	}

	parse_int32(args,"refclk",0,&refclk);

	if(refclk > 0)
		xtrx_set_ref_clk(h->dev,refclk,XTRX_CLKSRC_INT);

	h->tx_inter = 0;
	if(SRSRAN_SUCCESS == parse_uint32(args,"tx_inter",0,&tx_inter))
		h->tx_inter = tx_inter;

	h->rx_decim = 0;
	if(SRSRAN_SUCCESS == parse_uint32(args,"rx_decim",0,&rx_decim))
		h->rx_decim = rx_decim;

	h->rx_stream_enabled = false;
	h->tx_stream_enabled = false;

	/* Set default sampling rates */
	rf_xtrx_set_tx_srate(h,1.92e6);
	rf_xtrx_set_rx_srate(h,1.92e6);
	/* Set info structure */
	h->info.min_tx_gain = -52.0;
	h->info.max_tx_gain = 0.0;
	h->info.min_rx_gain = 0.0;
	h->info.max_rx_gain = 30.0;
	return(SRSRAN_SUCCESS);
}

int rf_xtrx_close(void *h_)
{
	int res = SRSRAN_SUCCESS;
	rf_xtrx_handler_t *h = h_;

	if(h != NULL)
	{
		res = xtrx_stop(h->dev,XTRX_TRX);
		xtrx_close(h->dev);
	}

	return(res);
}

static double rf_xtrx_set_srate_hz(rf_xtrx_handler_t *h,double srate_hz)
{
	int res = SRSRAN_SUCCESS;
	double cgen_freq = 0;
	double cgen_actual = 0;

	/* If the sampling rate is not modified dont bother */
	if(h->current_srate_hz == srate_hz)
		return(srate_hz);

	if((h->rx_decim > 1)
		&& (h->tx_inter > 1))
	{
		cgen_freq = srate_hz * 16 / (SRSRAN_MAX(h->rx_decim,h->tx_inter));
	}
	else
		cgen_freq = srate_hz * 32;

	res = xtrx_set_samplerate(h->dev,cgen_freq,srate_hz,srate_hz,
		(h->rx_decim) ? XTRX_SAMPLERATE_FORCE_RX_DECIM : 0,
		&cgen_actual,
		&h->rx_rate,
		&h->tx_rate);

	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_set_samplerate: can't deliver srate_hz %f,err: %d\n",srate_hz,res);
		return(SRSRAN_ERROR);
	}

	XTRX_RF_INFO("xtrx_set_samplerate srate_hz = %f master=%.3f MHz\n",srate_hz,cgen_actual / 1e6);
	XTRX_RF_INFO("rx_rate = %.2f Mhz tx_rate = %.2f Mhz\n",h->rx_rate / 1e6,h->tx_rate / 1e6);
	/* Update current sampling rate */
	h->current_srate_hz = srate_hz;
	return(srate_hz);
}

double rf_xtrx_set_rx_srate(void *h_,double freq)
{
	int res = 0;
	double actualbw;
	double srate_hz;
	rf_xtrx_handler_t *h = h_;

	srate_hz = rf_xtrx_set_srate_hz(h,freq);
	actualbw = 0;
	res = xtrx_tune_rx_bandwidth(h->dev,XTRX_CH_AB,srate_hz,&actualbw);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_tune_rx_bandwidth: can't rx bandwidth %f,err: %d\n",srate_hz,res);
	}
	else
	{
		XTRX_RF_INFO("xtrx_tune_rx_bandwidth rx bandwidth %.2f Mhz\n",(actualbw / 1e6));
	}

	return(srate_hz);
}

double rf_xtrx_set_tx_srate(void *h_,double freq)
{
	int res = 0;
	double actualbw;
	double srate_hz;
	rf_xtrx_handler_t *h = h_;

	srate_hz = rf_xtrx_set_srate_hz(h,freq);
	actualbw = 0;
	res = xtrx_tune_tx_bandwidth(h->dev,XTRX_CH_AB,srate_hz,&actualbw);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_tune_tx_bandwidth: can't tx bandwidth %f,err: %d\n",srate_hz,res);
	}
	else
	{
		XTRX_RF_INFO("xtrx_tune_tx_bandwidth tx bandwidth %.2f Mhz\n",(actualbw / 1e6));
	}

	return(srate_hz);
}

int rf_xtrx_set_rx_gain(void *h_,double gain)
{
	int res;
	double actual = 0;
	rf_xtrx_handler_t *h = h_;

	res = xtrx_set_gain(h->dev,XTRX_CH_AB,XTRX_RX_LNA_GAIN,gain,&actual);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_set_gain: can't rx gain %f,err: %d\n",gain,res);
	}
	else
	{
		XTRX_RF_INFO("xtrx_set_gain: rx gain %f\n",actual);
	}

	h->rx_gain = actual;
	return(SRSRAN_SUCCESS);
}

int rf_xtrx_set_rx_gain_ch(void *h,uint32_t ch,double gain)
{
	return(rf_xtrx_set_rx_gain(h,gain));
}

int rf_xtrx_set_tx_gain(void *h_,double gain)
{
	int res;
	double actual = 0;
	rf_xtrx_handler_t *h = h_;

	if(gain > 30.0)
		gain = 30.0;
	
	XTRX_RF_INFO("tx gain %f converted to (30.0-gain) = %f\n",gain,(gain - 30.0));
	gain = gain - 30.0;
	res = xtrx_set_gain(h->dev,XTRX_CH_AB,XTRX_TX_PAD_GAIN,gain,&actual);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_set_gain: can't tx gain %f,err: %d\n",gain,res);
	}
	else
	{
		XTRX_RF_INFO("xtrx_set_gain: tx gain %f\n",actual);
	}

	h->tx_gain = actual;
	return(SRSRAN_SUCCESS);
}

int rf_xtrx_set_tx_gain_ch(void *h,uint32_t ch,double gain)
{
	return(rf_xtrx_set_tx_gain(h,gain));
}

double rf_xtrx_get_rx_gain(void *h_)
{
	rf_xtrx_handler_t *h = h_;
	if(!h)
		return(0);

	return(h->rx_gain);
}

double rf_xtrx_get_tx_gain(void *h_)
{
	rf_xtrx_handler_t *h = h_;
	if(!h)
		return(0);

	return(h->tx_gain);
}

srsran_rf_info_t* rf_xtrx_get_info(void *h_)
{
	srsran_rf_info_t* info = NULL;

	if(h_)
	{
		rf_xtrx_handler_t *h = h_;
		info = &h->info;
	}

	return(info);
}

double rf_xtrx_set_rx_freq(void *h_,uint32_t ch,double freq)
{
	int res;
	double actual = 0;
	rf_xtrx_handler_t *h = h_;

	res = xtrx_tune(h->dev,XTRX_TUNE_RX_FDD,freq,&actual);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_tune: can't freq %f,err: %d\n",freq,res);
	}
	else
	{
		XTRX_RF_INFO("xtrx_tune: actual freq %.2f Mhz\n",(actual / 1e6));
	}

	return(actual);
}

double rf_xtrx_set_tx_freq(void *h_,uint32_t ch,double freq)
{
	int res;
	double actual = 0;
	rf_xtrx_handler_t *h = h_;

	res = xtrx_tune(h->dev,XTRX_TUNE_TX_FDD,freq,&actual);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_tune: can't freq %f,err: %d\n",freq,res);
	}
	else
	{
		XTRX_RF_INFO("xtrx_tune: actual freq %.2f Mhz\n",(actual / 1e6));
	}

	return(actual);
}

void rf_xtrx_get_time(void *h,time_t *secs,double *frac_secs)
{
}

int rf_xtrx_recv_with_time(void *h,void *data,uint32_t nsamples,bool blocking,time_t *secs,double *frac_secs)
{
	void *data_multi[SRSRAN_MAX_PORTS] = {NULL};
	data_multi[0] = data;

	return(rf_xtrx_recv_with_time_multi(h,data_multi,nsamples,blocking,secs,frac_secs));
}

int rf_xtrx_recv_with_time_multi(void *h_,void **data,uint32_t nsamples,bool blocking,time_t *secs,double *frac_secs)
{
	int res;
	rf_xtrx_handler_t *h = h_;
	struct xtrx_recv_ex_info ri;

	ri.samples = nsamples;
	ri.buffer_count = 1;
	ri.buffers = (void* const*)data;
	ri.flags = RCVEX_DONT_INSER_ZEROS;
	res = xtrx_recv_sync_ex(h->dev,&ri);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_recv_sync_ex returned %d\n",res);
		return(SRSRAN_ERROR);
	}

	if((secs != NULL)
		&& (frac_secs != NULL))
	{
		srsran_timestamp_t ts = {};
		srsran_timestamp_init_uint64(&ts,ri.out_first_sample,h->rx_rate);
		*secs = ts.full_secs;
		*frac_secs = ts.frac_secs;
		#if PRINT_RX_STATS
		XTRX_RF_INFO("rx_time: nsamples=%d tx_ts=%lu secs=%ld,frac_secs=%lf\n",nsamples,ri.out_first_sample,*secs,*frac_secs);
		#endif
	}

	return(nsamples);
}

int rf_xtrx_send_timed(void *h,void *data,int nsamples,time_t secs,double frac_secs,bool has_time_spec,bool blocking,bool is_start_of_burst,bool is_end_of_burst)
{
	void *_data[SRSRAN_MAX_PORTS] = {data,zero_mem,zero_mem,zero_mem};

	return(rf_xtrx_send_timed_multi(h,_data,nsamples,secs,frac_secs,has_time_spec,blocking,is_start_of_burst,is_end_of_burst));
}

int rf_xtrx_send_timed_multi(void *h_,void *data[SRSRAN_MAX_PORTS],int nsamples,time_t secs,double frac_secs,bool has_time_spec,bool blocking,bool is_start_of_burst,bool is_end_of_burst)
{
	int res;
	rf_xtrx_handler_t *h = h_;
	xtrx_send_ex_info_t nfo;
	static uint64_t tx_ts = 0;

	if(false == h->tx_stream_enabled)
		rf_xtrx_start_tx_stream(h);

	/* Convert time to ticks */
	if(has_time_spec)
	{
		srsran_timestamp_t ts = {.full_secs = secs,.frac_secs = frac_secs};
		tx_ts = srsran_timestamp_uint64(&ts,h->tx_rate);
	}
	else
		tx_ts += nsamples;	/* workaround for xtrx don't support tx without timestamp */

	#if PRINT_TX_STATS
	XTRX_RF_INFO("tx_time: nsamples=%d at tx_ts=%lu\n",nsamples,tx_ts);
	#endif

	nfo.buffers = (const void* const*)data;
	nfo.buffer_count = 1;
	nfo.flags = XTRX_TX_DONT_BUFFER;
	nfo.samples = nsamples;
	nfo.timeout = 100;
	nfo.ts = tx_ts;

	res = xtrx_send_sync_ex(h->dev,&nfo);
	if(res != 0)
	{
		XTRX_RF_ERROR("xtrx_send_sync_ex returned %d %lu\n",res,tx_ts);
		return(SRSRAN_ERROR);
	}

	return(SRSRAN_SUCCESS);
}

