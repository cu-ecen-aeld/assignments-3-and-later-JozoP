/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    size_t offset_counter = 0;
    size_t i;
    if(!buffer) //invalid input
    {
        return NULL;
    }
    
    i = buffer->out_offs;
    while (i != buffer->in_offs || (buffer->full && offset_counter == 0)) //loop through buffer until reaching last value in buffer
    {
        if(offset_counter +  buffer->entry[i].size > char_offset) //check if buffer->entry contains offset byte
        {
            *entry_offset_byte_rtn = char_offset - offset_counter; //matching offset found, set offset_byte to the selected byte in buffer->entry
            return &buffer->entry[i]; //return entry with the offset byte
        }
        offset_counter += buffer->entry[i].size; //increment byte offset
        i = (i + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; //buffer wrap
    }
    return NULL;

    // If the char_offset is not found in the buffer, return NULL
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
   if(!buffer | !add_entry) //invalid input
        return;
    
    buffer->entry[buffer->in_offs] = *(add_entry);
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    if (buffer->full)
    {
        buffer->out_offs = buffer->in_offs;
    }
    if (buffer->out_offs == buffer->in_offs)
    {
        buffer->full = true; //set full when head and tail index are the same
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

size_t aesd_get_total_size(struct aesd_circular_buffer *buffer)
{
    size_t total_size = 0;
    uint8_t max = 0;
    uint8_t idx;
    uint8_t pos;

    if (buffer->full)
    {
        max = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else
    {
        max = buffer->in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - buffer->out_offs;
        if (max >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            max -= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        }
    }

    pos = buffer->out_offs;
    for (idx = 0; idx < max; ++idx)
    {
        total_size += buffer->entry[pos].size;
        pos++;
        if (pos >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            pos = 0;
        }
    }

    return total_size;
}

long aesd_get_offset(struct aesd_circular_buffer *buffer, uint32_t write_cmd, uint32_t write_cmd_offset)
{
    size_t total_size = 0;
    uint8_t max = 0;
    uint8_t idx;
    uint8_t pos;

    if (buffer->full)
    {
        max = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else
    {
        max = buffer->in_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - buffer->out_offs;
        if (max >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            max -= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        }
    }

    if (write_cmd >= max)
    {
        return -1;
    }

    pos = buffer->out_offs;
    for (idx = 0; idx < write_cmd; ++idx)
    {
        total_size += buffer->entry[pos].size;
        pos++;
        if (pos >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
        {
            pos = 0;
        }
    }

    if (write_cmd_offset >= buffer->entry[pos].size)
    {
        return -1;
    }
    else
    {
        total_size += write_cmd_offset;
    }

    return total_size;
}