/*
 * circular_buffer.c
 * Functionality for a circular buffer/ringbuffer to be used on the receiving part of the serial communication with the system
 * The code is based mostly on
 *  Created on: Apr 26, 2021
 *      Author: Alexander.mizrahi@copenhagenatomics.com
 */



#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h> /* memset */
#include "circular_buffer.h"

// The definition of our circular buffer structure is hidden from the user
struct circular_buf_t {
	uint8_t * buffer;
	size_t head;
	size_t tail;
	size_t max; //of the buffer
	size_t noInputs;
	bool full;
};

//#pragma mark - Private Functions -

static void advance_pointer(cbuf_handle_t cbuf)
{
	assert(cbuf);

	if(circular_buf_full(cbuf))
    {
        cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    }

	cbuf->head = (cbuf->head + 1) % cbuf->max;

	// We mark full because we will advance tail on the next time around
	cbuf->full = (cbuf->head == cbuf->tail);
}

static void retreat_pointer(cbuf_handle_t cbuf)
{
	assert(cbuf);

	cbuf->full = false;
	cbuf->tail = (cbuf->tail + 1) % cbuf->max;
}

void circular_buf_add_input(cbuf_handle_t cbuf){
	assert(cbuf);
	cbuf->noInputs += 1;
}

void circular_buf_remove_input(cbuf_handle_t cbuf){
	assert(cbuf);
	cbuf->noInputs -= 1;
}

size_t circular_get_number_input(cbuf_handle_t cbuf){
	assert(cbuf);
	return cbuf->noInputs;
}

void circular_read_command(cbuf_handle_t cbuf, uint8_t *tmpBuf){

	// If no inputs are available
	if (circular_get_number_input(cbuf)==0){
		tmpBuf[0]='\0';
		return;
	}

	// Read command
	uint16_t i = 0;
	while(!circular_buf_empty(cbuf)){
		uint8_t data;
		circular_buf_get(cbuf, &data);
		// Checking for '\r' is there for debugging purposes which is sent by putty and minicom.
		// In production only '\n' is sent. Therefore, there is no check for '\r\n' case.
		if (data=='\n'||data=='\r'){
			tmpBuf[i] = '\0';
			circular_buf_remove_input(cbuf);
			break;
		}
		tmpBuf[i] = data;
		i++;
	}
	return;
}


//#pragma mark - APIs -

cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size)
{
	assert(buffer && size);

	cbuf_handle_t cbuf = malloc(sizeof(circular_buf_t) + sizeof(buffer));
	assert(cbuf);

	cbuf->buffer = buffer;
	cbuf->max = size;
	circular_buf_reset(cbuf);

	assert(circular_buf_empty(cbuf));

	return cbuf;
}

void circular_buf_free(cbuf_handle_t cbuf)
{
	assert(cbuf);
	free(cbuf);
}

void circular_buf_reset(cbuf_handle_t cbuf)
{
    assert(cbuf);

    cbuf->head = 0;
    cbuf->tail = 0;
	cbuf->noInputs = 0;
    cbuf->full = false;
}

size_t circular_buf_size(cbuf_handle_t cbuf)
{
	assert(cbuf);

	size_t size = cbuf->max;

	if(!circular_buf_full(cbuf))
	{
		if(cbuf->head >= cbuf->tail)
		{
			size = (cbuf->head - cbuf->tail);
		}
		else
		{
			size = (cbuf->max + cbuf->head - cbuf->tail);
		}

	}

	return size;
}

size_t circular_buf_capacity(cbuf_handle_t cbuf)
{
	assert(cbuf);

	return cbuf->max;
}

void circular_buf_put(cbuf_handle_t cbuf, uint8_t data)
{
	assert(cbuf && cbuf->buffer);

    cbuf->buffer[cbuf->head] = data;

    advance_pointer(cbuf);
}

int circular_buf_put2(cbuf_handle_t cbuf, uint8_t data)
{
    int r = -1;

    assert(cbuf && cbuf->buffer);

    if(!circular_buf_full(cbuf))
    {
        cbuf->buffer[cbuf->head] = data;
        advance_pointer(cbuf);
        r = 0;
    }

    return r;
}

int circular_buf_get(cbuf_handle_t cbuf, uint8_t * data)
{
    assert(cbuf && data && cbuf->buffer);

    int r = -1;

    if(!circular_buf_empty(cbuf))
    {
        *data = cbuf->buffer[cbuf->tail];
        retreat_pointer(cbuf);

        r = 0;
    }

    return r;
}

bool circular_buf_empty(cbuf_handle_t cbuf)
{
	assert(cbuf);

    return (!circular_buf_full(cbuf) && (cbuf->head == cbuf->tail));
}

bool circular_buf_full(cbuf_handle_t cbuf)
{
	assert(cbuf);

    return cbuf->full;
}
