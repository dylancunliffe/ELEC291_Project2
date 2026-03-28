#ifndef INTERSECTION_H
#define INTERSECTION_H

#include <stdint.h>

typedef enum {
    MOVE_FORWARD,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_STOP
} MoveType;

MoveType Get_Move(uint8_t intersection, uint8_t path);
void Follow_Line(int32_t error);
void Move_Start(MoveType move);
void Move_Update(void);
uint8_t Move_IsDone(void);


#endif
