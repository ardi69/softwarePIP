/*
* ringbuffer.h: A ring buffer
*
* See the main source file 'vdr.c' for copyright information and
* how to reach the author.
*
* $Id: ringbuffer.h 2.3 2011/12/04 13:38:17 kls Exp $
*/

#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include <thread>
#include <mutex>
#include "tools.h"

class cRingBuffer {
private:
	cEvent readyForPut, readyForGet;
	int putTimeout;
	int getTimeout;
	int size;
	time_t lastOverflowReport;
	int overflowCount;
	int overflowBytes;
protected:
//	tThreadId getThreadTid;
	int maxFill;//XXX
	int lastPercent;
	bool statistics;//XXX
	void UpdatePercentage(int Fill);
	void WaitForPut(void);
	void WaitForGet(void);
	void EnablePut(void);
	void EnableGet(void);
	virtual void Clear(void) = 0;
	virtual int Available(void) = 0;
	virtual int Free(void) { return Size() - Available() - 1; }
	int Size(void) { return size; }
public:
	cRingBuffer(int Size, bool Statistics = false);
	virtual ~cRingBuffer();
	void SetTimeouts(int PutTimeout, int GetTimeout);
	void ReportOverflow(int Bytes);
};

class cRingBufferLinear : public cRingBuffer {
	//#define DEBUGRINGBUFFERS
#ifdef DEBUGRINGBUFFERS
private:
	int lastHead, lastTail;
	int lastPut, lastGet;
	static cRingBufferLinear *RBLS[];
	static void AddDebugRBL(cRingBufferLinear *RBL);
	static void DelDebugRBL(cRingBufferLinear *RBL);
public:
	static void PrintDebugRBL(void);
#endif
private:
	int margin, head, tail;
	int gotten;
	uint8_t *buffer;
	char *description;
protected:
	virtual int DataReady(const uint8_t *Data, int Count);
	///< By default a ring buffer has data ready as soon as there are at least
	///< 'margin' bytes available. A derived class can reimplement this function
	///< if it has other conditions that define when data is ready.
	///< The return value is either 0 if there is not yet enough data available,
	///< or the number of bytes from the beginning of Data that are "ready".
public:
	cRingBufferLinear(int Size, int Margin = 0, bool Statistics = false, const char *Description = NULL);
	///< Creates a linear ring buffer.
	///< The buffer will be able to hold at most Size-Margin-1 bytes of data, and will
	///< be guaranteed to return at least Margin bytes in one consecutive block.
	///< The optional Description is used for debugging only.
	virtual ~cRingBufferLinear();
	virtual int Available(void);
	virtual int Free(void) { return Size() - Available() - 1 - margin; }
	virtual void Clear(void);
	///< Immediately clears the ring buffer.
	int Read(int FileHandle, int Max = 0);
	///< Reads at most Max bytes from FileHandle and stores them in the
	///< ring buffer. If Max is 0, reads as many bytes as possible.
	///< Only one actual read() call is done.
	///< \return Returns the number of bytes actually read and stored, or
	///< an error value from the actual read() call.
	int Read(FILE *File, int Max = 0);
	///< Like Read(int FileHandle, int Max), but reads from a FILE*).
	int Put(const uint8_t *Data, int Count);
	///< Puts at most Count bytes of Data into the ring buffer.
	///< \return Returns the number of bytes actually stored.
	uint8_t *Get(int &Count);
	///< Gets data from the ring buffer.
	///< The data will remain in the buffer until a call to Del() deletes it.
	///< \return Returns a pointer to the data, and stores the number of bytes
	///< actually available in Count. If the returned pointer is NULL, Count has no meaning.
	void Del(int Count);
	///< Deletes at most Count bytes from the ring buffer.
	///< Count must be less or equal to the number that was returned by a previous
	///< call to Get().
};

enum eFrameType { ftUnknown, ftVideo, ftAudio, ftDolby };
extern "C" {
	struct AVPacket;
	typedef struct AVPacket AVPacket;
}
class cFrame {
	friend class cRingBufferFrame;
private:
	cFrame *next;
	AVPacket *data;
	int count;
	ePictureType type;
public:
	cFrame(const uint8_t *Data, int Count, ePictureType PictureType);
	///< Creates a new cFrame object.
	///< If Count is negative, the cFrame object will take ownership of the given
	///< Data. Otherwise it will allocate Count bytes of memory and copy Data.
	~cFrame();
	AVPacket *Data(void) const { return data; }
	int Count(void) const { return count; }
	ePictureType Type(void) const { return type; }
};

class cRingBufferFrame : public cRingBuffer {
private:
	std::recursive_mutex mutex;
	cFrame *head;
	int currentFill;
	int currentIFrames;
	void Delete(cFrame *Frame);
	void Lock(void) { mutex.lock(); }
	void Unlock(void) { mutex.unlock(); }
public:
	cRingBufferFrame(int Size, bool Statistics = false);
	virtual ~cRingBufferFrame();
	virtual int Available(void);
	virtual void Clear(void);
	int IFrames();
	// Immediately clears the ring buffer.
	bool Put(cFrame *Frame);
	// Puts the Frame into the ring buffer.
	// Returns true if this was possible.
	cFrame *Get(void);
	// Gets the next frame from the ring buffer.
	// The actual data still remains in the buffer until Drop() is called.
	void Drop(cFrame *Frame);
	// Drops the Frame that has just been fetched with Get().
};

#endif // __RINGBUFFER_H
