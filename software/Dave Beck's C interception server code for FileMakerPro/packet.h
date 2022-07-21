/*******************************************************************************
** packet.h
** UDP packet header
** David Andrew Crawford Beck
** dacb@uw.edu
** Original:
**	Mon Apr 1 10:30:16 PDT 2013
** Modified:
**  Thu Jan 15 01:46:23 PST 2015
**   Modified by Jonathan Forbes to account for 64 ADCs in the new system
*******************************************************************************/

#ifndef __PACKET_H__
#define __PACKET_H__

#define ADCs 64

typedef struct packet {
	uint32_t sequenceID;
	uint16_t adc[ADCs];
} packet_t;

#endif /* __PACKET_H__ */