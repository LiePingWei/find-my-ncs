#ifndef FMNA_H_
#define FMNA_H_

#ifdef __cplusplus
extern "C" {
#endif

struct fmna_init_params {
	uint8_t bt_id;
	bool use_default_factory_settings;
};

int fmna_init(const struct fmna_init_params *init_params);

int fmna_serial_number_lookup_enable(void);

#ifdef __cplusplus
}
#endif


#endif /* FMNA_H_ */