#ifndef IR_R_H
#define IR_R_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


extern uint8_t tx_buffer[10];
extern uint8_t flag_buffer[10];
extern uint8_t second_buffer[8];
extern uint32_t final_buffer[40];
extern volatile int printnow;
extern volatile int printnow2;
extern uint32_t timer21_value;
extern volatile uint32_t overflow_timer21;
extern volatile int countup;
extern volatile uint32_t ir_signal;
extern volatile uint32_t ir_signal_copy;
extern volatile uint16_t direction_data;
extern volatile uint16_t rotation_data;
extern volatile uint16_t preset_data;

void get_tim21_value(void);
void populate_data(void);
void do_on_negedge (void);
void decode_into_flags (ir);

#endif

