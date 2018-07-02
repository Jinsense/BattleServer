#include "EtherNet_PDH.h"


//	PdhEnumObjectItems을 통해서 NetworkInterface 항목에서 얻을 수 있는
//	측성항목(Counters) / 인터페이스 항목을 얻음. 그런데 그 개수나 길이를 모르기 때문에 
//	먼저 버퍼의 길이를 알기 위해서 Out Buffer 인자들을 NULL 포인터로 넣어서 사이즈만 확인
CEthernet::CEthernet()
{
	PdhOpenQuery(NULL, NULL, &_pdh_Query);
	PdhCollectQueryData(_pdh_Query);
	Init();
	Count();
}

CEthernet::~CEthernet()
{
	delete[] szCounters;
	delete[] szInterfaces;
}

bool CEthernet::Init()
{
	PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);
	szCounters = new WCHAR[dwCounterSize];
	szInterfaces = new WCHAR[dwInterfaceSize];

	if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0)
		!= ERROR_SUCCESS)
	{
		delete[] szCounters;
		delete[] szInterfaces;
		return false;
	}
	iCnt = 0;
	szCur = szInterfaces;

	for (; *szCur != L'\0' && iCnt < df_PDH_ETHERNET_MAX; szCur += wcslen(szCur) + 1, iCnt++)
	{
		_EthernetStruct[iCnt]._bUse = true;
		_EthernetStruct[iCnt]._szName[0] = L'\0';

		wcscpy_s(_EthernetStruct[iCnt]._szName, szCur);
		//		strcpy_s(_EthernetStruct[iCnt]._szName, szCur);

		szQuery[0] = L'\0';
		StringCchPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Received/sec", szCur);
		_pdh_Status = PdhAddCounter(_pdh_Query, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes);
		if (_pdh_Status != 0)
			wprintf(L"Format message failed with 0x%x\n", GetLastError());

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Sent/sec", szCur);
		_pdh_Status = PdhAddCounter(_pdh_Query, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes);
	}
	return true;
}

void CEthernet::Count()
{
	for (int iCnt = 0; iCnt < df_PDH_ETHERNET_MAX; iCnt++)
	{
		if (true == _EthernetStruct[iCnt]._bUse)
		{
			PdhCollectQueryData(_pdh_Query);
			_pdh_Status = PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes, PDH_FMT_DOUBLE, NULL, &_CounterValue);
			if (_pdh_Status == 0) _pdh_value_Network_RecvBytes += _CounterValue.doubleValue;

			_pdh_Status = PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes, PDH_FMT_DOUBLE, NULL, &_CounterValue);
			if (_pdh_Status == 0) _pdh_value_Network_SendBytes += _CounterValue.doubleValue;
		}
	}
	return;
}

