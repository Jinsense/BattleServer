#include "Packet.h"

#include <Windows.h>

//CMemoryPool<CPacket>* CPacket::m_pMemoryPool = NULL;
CMemoryPoolTLS<CPacket>* CPacket::m_pMemoryPool = NULL;
BYTE CPacket::_byCode = NULL;
BYTE CPacket::_byPacketKey1 = NULL;
BYTE CPacket::_byPacketKey2 = NULL;
long CPacket::_UseCount = 0;

CPacket::CPacket() :
	_iBufferSize(static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE)),
	_pEndPos(_chBuffer + static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE))
{
	ZeroMemory(&_chBuffer, static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE));
	_iDataSize = 0;
	_pReadPos = _chBuffer;
	_pWritePos = _chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
	_lHeaderSetFlag = false;
	_iRefCount = 0;
}

CPacket::~CPacket()
{
}

void CPacket::Clear()
{
	ZeroMemory(&_chBuffer, static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE));
	_iDataSize = 0;
	_pReadPos = _chBuffer;
	_pWritePos = _chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
	_lHeaderSetFlag = false;
	_iRefCount = 0;
	_iBufferSize = static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE);
	_pEndPos = _chBuffer + static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE);
}

CPacket * CPacket::Alloc()
{
	InterlockedIncrement(&_UseCount);

	CPacket *_pPacket = m_pMemoryPool->Alloc();
	_pPacket->Clear();
	_pPacket->AddRef();
	return _pPacket;
}

void CPacket::Free()
{
	__int64 Count = InterlockedDecrement64(&_iRefCount);
	if (0 >= Count)
	{
		if (0 > Count)
			g_CrashDump->Crash();
		else
		{
			InterlockedDecrement(&_UseCount);
			m_pMemoryPool->Free(this);
		}
	}
}

void CPacket::MemoryPoolInit()
{
	if (m_pMemoryPool == nullptr)
		m_pMemoryPool = new CMemoryPoolTLS<CPacket>();
}

void CPacket::Init(BYTE byCode, BYTE byPacketKey1, BYTE byPacketKey2)
{
	_byCode = byCode;
	_byPacketKey1 = byPacketKey1;
	_byPacketKey2 = byPacketKey2;
	return;
}

void CPacket::AddRef()
{
	InterlockedIncrement64(&_iRefCount);
}

void CPacket::PushData(WCHAR* pSrc, int iSize)
{
	int Size = iSize * 2;
	if (_pEndPos - _pWritePos < Size)
		g_CrashDump->Crash();
	/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR),
	Size, int(m_pEndPos - m_pWritePos));*/
	memcpy_s(_pWritePos, Size, pSrc, Size);
	_pWritePos += Size;
	_iDataSize += Size;
}

void CPacket::PopData(WCHAR* pDest, int iSize)
{
	int Size = iSize * 2;
	if (_iDataSize < Size)
		g_CrashDump->Crash();
	/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::POP_ERR),
	Size, m_iDataSize);*/
	memcpy_s(pDest, Size, _pReadPos, Size);
	_pReadPos += Size;
	_iDataSize -= Size;
}

void CPacket::PushData(char *pSrc, int iSize)
{
	if (_pEndPos - _pWritePos < iSize)
		g_CrashDump->Crash();
	/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR),
	iSize, int(m_pEndPos - m_pWritePos));*/
	memcpy_s(_pWritePos, iSize, pSrc, iSize);
	_pWritePos += iSize;
	_iDataSize += iSize;
}

void CPacket::PopData(char *pDest, int iSize)
{
	if (_iDataSize < iSize)
		g_CrashDump->Crash();
	/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::POP_ERR),
	iSize, m_iDataSize);*/
	memcpy_s(pDest, iSize, _pReadPos, iSize);
	_pReadPos += iSize;
	_iDataSize -= iSize;
}

void CPacket::PushData(int iSize)
{
	if (_pEndPos - _pWritePos < iSize)
		g_CrashDump->Crash();
	/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR),
	iSize, int(m_pEndPos - m_pWritePos));*/
	_pWritePos += iSize;
	_iDataSize += iSize;
}

void CPacket::PopData(int iSize)
{
	if (_iDataSize < iSize)
		g_CrashDump->Crash();
	/*throw st_ERR_INFO(static_cast<int>(en_PACKETDEFINE::PUSH_ERR),
	iSize, m_iDataSize);*/
	_pReadPos += iSize;
	_iDataSize -= iSize;
}

void CPacket::SetHeader(char *pHeader)
{
	if (true == InterlockedCompareExchange(&_lHeaderSetFlag, true, false))
		return;
	memcpy_s(_chBuffer, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE),
		pHeader, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE));
}

void CPacket::SetHeader_CustomHeader(char *pHeader, int iCustomHeaderSize)
{
	if (true == InterlockedCompareExchange(&_lHeaderSetFlag, true, false))
		return;
	int iSize = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE) - iCustomHeaderSize;
	memcpy_s(&_chBuffer[iSize], static_cast<int>(en_PACKETDEFINE::HEADER_SIZE),
		pHeader, iCustomHeaderSize);
}

void CPacket::SetHeader_CustomShort(unsigned short shHeader)
{
	if (true == InterlockedCompareExchange(&_lHeaderSetFlag, true, false))
		return;
	memcpy_s(&_chBuffer[3], static_cast<int>(en_PACKETDEFINE::SHORT_HEADER_SIZE),
		&shHeader, static_cast<int>(en_PACKETDEFINE::SHORT_HEADER_SIZE));

	_pReadPos += 3;
}

void CPacket::EnCode()
{
	if (true == InterlockedCompareExchange(&_lHeaderSetFlag, true, false))
		return;

	st_PACKET_HEADER Header;
	Header.shLen = _iDataSize;

	int iCheckSum = 0;
	BYTE *pPtr = (BYTE*)&_chBuffer[5];

	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
		iCnt < _iDataSize + sizeof(st_PACKET_HEADER); iCnt++)
	{
		iCheckSum += *pPtr;
		pPtr++;
	}
	Header.CheckSum = (BYTE)(iCheckSum % 256);
	Header.CheckSum = Header.CheckSum ^ Header.RandKey ^ _byPacketKey1 ^ _byPacketKey2;
	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
		iCnt < _iDataSize + sizeof(st_PACKET_HEADER); iCnt++)
		_chBuffer[iCnt] = _chBuffer[iCnt] ^ Header.RandKey ^ _byPacketKey1 ^ _byPacketKey2;
	Header.RandKey = Header.RandKey ^ _byPacketKey1 ^ _byPacketKey2;
	memcpy_s(&_chBuffer, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE),
		&Header, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE));

	return;
}

bool CPacket::DeCode(st_PACKET_HEADER * pInHeader)
{
	if (nullptr == pInHeader)
		memcpy_s(&pInHeader, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE),
			_chBuffer, static_cast<int>(en_PACKETDEFINE::HEADER_SIZE));

	if (pInHeader->byCode != _byCode)
		return false;

	pInHeader->RandKey = pInHeader->RandKey ^ _byPacketKey1 ^ _byPacketKey2;
	pInHeader->CheckSum = pInHeader->CheckSum ^ pInHeader->RandKey ^ _byPacketKey1 ^ _byPacketKey2;
	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
		iCnt < pInHeader->shLen + sizeof(st_PACKET_HEADER); iCnt++)
	{
		_chBuffer[iCnt] = _chBuffer[iCnt] ^ pInHeader->RandKey ^ _byPacketKey1 ^ _byPacketKey2;
	}
	int iCheckSum = 0;
	BYTE *pPtr = (BYTE*)&_chBuffer[5];
	for (int iCnt = static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
		iCnt < static_cast<int>(en_PACKETDEFINE::HEADER_SIZE) + pInHeader->shLen; iCnt++)
	{
		iCheckSum += *pPtr;
		pPtr++;
	}
	iCheckSum = (BYTE)(iCheckSum % 256);

	if (iCheckSum != pInHeader->CheckSum)
		return false;

	if (pInHeader->shLen >= static_cast<int>(en_PACKETDEFINE::BUFFER_SIZE))
		return false;

	return true;
}

CPacket& CPacket::operator=(CPacket& Packet)
{
	_iDataSize = Packet._iDataSize;
	_pWritePos = _chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE) +
		_iDataSize;
	_pReadPos = _chBuffer + static_cast<int>(en_PACKETDEFINE::HEADER_SIZE);
	memcpy_s(_chBuffer, _iDataSize, Packet._pReadPos, _iDataSize);

	return *this;
}

CPacket& CPacket::operator<<(char Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned char Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(short Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned short Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(int Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned int Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(long Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(unsigned long Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(float Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(__int64 Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator<<(double Value)
{
	PushData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (char& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned char& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (short& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned short& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (int& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned int& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (long& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (unsigned long& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (float& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (__int64& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}

CPacket& CPacket::operator >> (double& Value)
{
	PopData((char*)&Value, sizeof(Value));
	return *this;
}