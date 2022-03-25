#include <stdbool.h>
#include <stdint.h>

#include "srsran/config.h"
#include "srsran/phy/rf/rf.h"

#define DEVNAME "xtrx"
#define ANALOG_LPF_COEFF	1

enum xtrx_loglevel
{
	XTRX_LOG_NONE = 0,
	XTRX_LOG_ERROR,
	XTRX_LOG_WARNING,
	XTRX_LOG_INFO,
	XTRX_LOG_INFO_LMS,
	XTRX_LOG_DEBUG,
	XTRX_LOG_DEBUG_REGS,
	XTRX_LOG_PARANOIC,
};

#define XTRX_RF_INFO(...) \
do \
{ \
	if(rf_xtrx_logging_level >= XTRX_LOG_INFO) \
	{ \
		fprintf(stdout,"[XTRX RF INFO] " __VA_ARGS__); \
	} \
} while(false)

#define XTRX_RF_DEBUG(...) \
do \
{ \
	if(rf_xtrx_logging_level >= XTRX_LOG_DEBUG) \
	{ \
		fprintf(stdout,"[XTRX RF INFO] " __VA_ARGS__); \
	} \
} while(false)

#define XTRX_RF_ERROR(...) \
do \
{ \
	if(rf_xtrx_logging_level >= XTRX_LOG_ERROR) \
	{ \
		fprintf(stdout,"[XTRX RF ERROR] " __VA_ARGS__); \
	} \
} while(false)

void rf_xtrx_suppress_stdout(void *h);
void rf_xtrx_register_error_handler(void *h,srsran_rf_error_handler_t error_handler,void *arg);
const char* rf_xtrx_devname(void *h);
int rf_xtrx_start_tx_stream(void *h);
int rf_xtrx_start_rx_stream(void *h,bool now);
int rf_xtrx_stop_rx_stream(void *h);
void rf_xtrx_flush_buffer(void *h);
bool rf_xtrx_has_rssi(void *h);
float rf_xtrx_get_rssi(void *h);
int rf_xtrx_open_multi(char* args,void **handler,uint32_t nof_channels);
int rf_xtrx_open(char *args,void **handler);
int rf_xtrx_close(void *h);
double rf_xtrx_set_rx_srate(void *h,double freq);
double rf_xtrx_set_tx_srate(void *h,double freq);
int rf_xtrx_set_rx_gain(void *h,double gain);
int rf_xtrx_set_rx_gain_ch(void *h,uint32_t ch,double gain);
int rf_xtrx_set_tx_gain(void *h,double gain);
int rf_xtrx_set_tx_gain_ch(void *h,uint32_t ch,double gain);
double rf_xtrx_get_rx_gain(void *h);
double rf_xtrx_get_tx_gain(void *h);
srsran_rf_info_t* rf_xtrx_get_info(void *h);
double rf_xtrx_set_rx_freq(void *h,uint32_t ch,double freq);
double rf_xtrx_set_tx_freq(void *h,uint32_t ch,double freq);
void rf_xtrx_get_time(void *h,time_t *secs,double *frac_secs);
int rf_xtrx_recv_with_time_multi(void *h,void** data,uint32_t nsamples,bool blocking,time_t* secs,double* frac_secs);
int rf_xtrx_recv_with_time(void *h,void *data,uint32_t nsamples,bool blocking,time_t* secs,double* frac_secs);
int rf_xtrx_send_timed_multi(void *h,void *data[4],int nsamples,time_t secs,double frac_secs,bool has_time_spec,bool blocking,bool is_start_of_burst,bool is_end_of_burst);
int rf_xtrx_send_timed(void *h,void *data_,int nsamples,time_t secs,double frac_secs,bool has_time_spec,bool blocking,bool is_start_of_burst,bool is_end_of_burst);

