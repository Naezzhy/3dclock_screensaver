/*
 *  Copyright (c) 2013 - 2018 Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * File:   discrete_ring_buffer.h
 * Author: Naezzhy Petr(Наезжий Пётр) <petn@mail.ru>
 *
 * Created on 10 апреля 2018 г., 9:29
 */

#ifndef DISCRETE_RING_BUFFER_H
#define DISCRETE_RING_BUFFER_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>



/******************************************************************************/
/***************** Multithread safe Ring Buffer to accumulate *****************/
/************************* different data chanks ******************************/
/******************************************************************************/
class cDiscreteRingBuffer
{
public:
	cDiscreteRingBuffer();
	virtual ~cDiscreteRingBuffer();
	int32_t	create(const size_t uChunkSize, const uint8_t uBuffLen);
	void	destroy(void);
	int32_t write(void const *buffer, size_t bufferSize);
	int32_t read(void *buffer, const size_t bufferSize, size_t *dataSize);
	size_t	get_current_len();
//	int32_t resize(uint32_t uNewMaxLen);

private:
	uint8_t static const MAX_LENGTH = 254;
	uint8_t static const MIN_LENGTH = 2;
	size_t	pSizeBuffer[MAX_LENGTH];
	size_t	uChunkSize;
	uint8_t *pBuffer;
	uint8_t uHead;
	uint8_t uTail;
	uint8_t uLen;
	uint8_t uMaxLen;

	void move_head(void);
	void move_tail(void);

};

cDiscreteRingBuffer::cDiscreteRingBuffer()
{
	pBuffer = NULL;
	uHead = 0;
	uTail = 0;
	uLen = 0;
	uMaxLen = 0;

}

cDiscreteRingBuffer::~cDiscreteRingBuffer()
{
	destroy();
}

int32_t cDiscreteRingBuffer::
create(const size_t uChunkSize_, const uint8_t uBuffLen_)
{
	if(uChunkSize_ == 0)
	{
		fprintf(stderr, "init: Invalid input");
		return -1;
	}

	if (uBuffLen_ < MIN_LENGTH)
		uMaxLen = MIN_LENGTH;
	else if (uBuffLen_ > MAX_LENGTH)
		uMaxLen = MAX_LENGTH;
	else
		uMaxLen = uBuffLen_;
	
	uChunkSize = uChunkSize_;
	uHead = 0;
	uTail = 0;
	uLen = 0;
	
	if(pBuffer != NULL)
	{
		delete[] pBuffer;
		pBuffer = NULL;
	}
	
	
	pBuffer = new uint8_t[uChunkSize*uMaxLen];
	if(pBuffer == NULL)
	{
		fprintf(stderr, "init: Error memory allocation");
		return -1;
	}
	
	memset(pBuffer, 0, uChunkSize*uMaxLen);
	
	return 1;
}

size_t cDiscreteRingBuffer::get_current_len()
{
	return uLen;
}

void cDiscreteRingBuffer::
destroy()
{
	if(pBuffer != NULL)
	{
		delete[] pBuffer;
		pBuffer = NULL;
	}
}

void cDiscreteRingBuffer::
move_head()
{
	uHead++;
	if (uHead >= uMaxLen)
		uHead = 0;

	uLen++;
	if (uLen >= uMaxLen)
		uLen = uMaxLen;
}

void cDiscreteRingBuffer::move_tail()
{
	uTail++;
	if (uTail >= uMaxLen)
		uTail = 0;

	uLen--;
}

//int32_t cDiscreteRingBuffer::resize(uint32_t uNewMaxLen)
//{
//	if( NULL == pBuffer)
//		return -1;
//	
//	if (uNewMaxLen == uMaxLen)
//		return 0;
//	else if (uNewMaxLen < MIN_LENGTH)
//		uNewMaxLen = MIN_LENGTH;
//	else if (uNewMaxLen > MAX_LENGTH)
//		uNewMaxLen = MAX_LENGTH;
//	
//	uint8_t *pNewBuffer = new uint8_t[uNewMaxLen*uChunkSize];
//	
//	if(uNewMaxLen > uMaxLen)
//		memcpy(pNewBuffer, pBuffer, uMaxLen*uChunkSize);
//	else
//		memcpy(pNewBuffer, pBuffer, uNewMaxLen*uChunkSize);
//	
//	uMaxLen = uNewMaxLen;
//
//	uHead = 0;
//	uTail = 0;
//	uLen = 0;
//	uMaxLen = uNewMaxLen;
//
//	memset(pSizeBuffer, 0, sizeof (pSizeBuffer));
//
//	return 1;
//}

int32_t cDiscreteRingBuffer::write(const void *buffer, size_t dataSize)
{
	if ( (buffer == NULL) || (dataSize == 0) )
		return -1;

	/* Too big buffer */
	if ( dataSize > uChunkSize )
		dataSize = uChunkSize;

//	if ((uHead == uTail) && (0 != pSizeBuffer[uHead]))
//		move_head();
	/* Jump over tail */
	if ((uHead == uTail) && (0 != pSizeBuffer[uHead]))
	{
		uHead = 0;
		uTail = 0;
		uLen = 0;
		memset(pSizeBuffer, 0, sizeof(pSizeBuffer));
//		fprintf(stderr, "********** Jump over tail *************\r\n");
	}
//	/* Jump over tail */
//	if ((uHead == uTail) && (0 != pSizeBuffer[uHead]))
//	{
//		uLen = 1;
//		move_head();
//		fprintf(stderr, "********** Jump over tail *************\r\n");
//	}

	memcpy(pBuffer + (uHead * uChunkSize), buffer, dataSize);
	pSizeBuffer[uHead] = dataSize;

	move_head();

	return 1;
}

int32_t cDiscreteRingBuffer::read(void *buffer, const size_t bufferSize, size_t* dataSize)
{
	if ( buffer == NULL || dataSize == NULL || bufferSize == 0 )
		return -1;
	
	/* Too small buffer */
	if( bufferSize < pSizeBuffer[uTail] )
		*dataSize = bufferSize;
	else
		*dataSize = pSizeBuffer[uTail];

	if (0 == *dataSize || uLen < 1)
		return 0;

	memcpy(buffer, pBuffer + (uTail * uChunkSize), *dataSize);
	pSizeBuffer[uTail] = 0;

	move_tail();

	return 1;
}

#endif /* DISCRETE_RING_BUFFER_H */

