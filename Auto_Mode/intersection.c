#include "intersection.h"
#include "sensors.h"
#include "motors.h"
#include "main.h"
#include <stdio.h>

static MoveType current_move;
static uint8_t move_done;


static const MoveType preset_table[8][3] = {

    // Path1,        Path2,        Path3
    {MOVE_FORWARD,   MOVE_LEFT,    MOVE_RIGHT},   // 1
    {MOVE_LEFT,      MOVE_RIGHT,   MOVE_FORWARD}, // 2
    {MOVE_LEFT,      MOVE_LEFT,    MOVE_RIGHT},   // 3
    {MOVE_FORWARD,   MOVE_RIGHT,   MOVE_LEFT},    // 4
    {MOVE_RIGHT,     MOVE_FORWARD, MOVE_RIGHT},   // 5
    {MOVE_LEFT,      MOVE_FORWARD, MOVE_LEFT},    // 6
    {MOVE_RIGHT,     MOVE_STOP,    MOVE_FORWARD}, // 7
    {MOVE_STOP,      MOVE_STOP,    MOVE_STOP}     // 8
};


MoveType Get_Move(uint8_t intersection, uint8_t path)
{
    // safety checks
    if(intersection > 8)
        return MOVE_STOP;

    return preset_table[intersection - 1][path];
}

void Move_Start(MoveType move)
{
    current_move = move;
    move_done = 0;
}

#define CENTER_THRESHOLD 200 //need to change this later after sensors are on the car

void Move_Update(void)
{
    switch(current_move)
    {
        case MOVE_FORWARD:
        {
        	int32_t error = Sensors_Get_Error();
        	            Follow_Line(error);
        	            break;
        }


        case MOVE_LEFT:

        	Motors_SetSpeed(0, 200);

            if(Sensors_Get_Center() > CENTER_THRESHOLD)
                move_done = 1;
            break;

        case MOVE_RIGHT:
            Motors_SetSpeed(200, 0);

            if(Sensors_Get_Center() > CENTER_THRESHOLD)
                move_done = 1;
            break;

        case MOVE_STOP:
        	Motors_Stop(); //assuming we have this function
            move_done = 1;
            break;
    }
}

uint8_t Move_IsDone(void)
{
    return move_done;
}

// option for bonus points, can add coloured LED's as blinkers
