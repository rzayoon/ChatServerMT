#pragma once

#include <Pdh.h>




class CPDH
{
	enum
	{
		e_PDH_ETHERNET_MAX = 8
	};

	struct st_ETHERNET
	{
		bool _bUse;
		wchar_t _szName[128];

		PDH_HCOUNTER _pdh_Counter_Network_RecvBytes;
		PDH_HCOUNTER _pdh_Counter_Network_SendBytes;

	};

public:
	CPDH();
	virtual ~CPDH()
	{

	}

	bool Init();

	void Collect();

	long GetPrivateBytes() { return _privateBytes; }
	long GetProcessNonPaged() { return _processNonPaged; }
	long GetTotalNonpaged() { return _totalNonPaged; }
	long GetAvailableMBytes() { return _availMB; }

	long GetRecvBytes() { return _pdh_value_Network_RecvBytes; }
	long GetSendBytes() { return _pdh_value_Network_SendBytes; }

	int GetPrivateMBytes() { return BytesToMB(_privateBytes); }
	int GetTotalNonpagedMB() { return BytesToMB(_totalNonPaged); }

	int BytesToMB(unsigned int bytes) { return ((unsigned)bytes & 0xfff00000) >> 20; }
	int BytesToKB(unsigned int bytes) { return ((unsigned)bytes & 0xfffffc00) >> 10; }

	int GetRecvKBytes() { return BytesToKB(_pdh_value_Network_RecvBytes); }
	int GetSendKBytes() { return BytesToKB(_pdh_value_Network_SendBytes); }

	void Show();

private:

	PDH_HQUERY _hQuery;
	PDH_HCOUNTER _PDHprivateBytes;
	PDH_HCOUNTER _PDHprocessNonPaged;
	PDH_HCOUNTER _PDHtotalNonPaged;
	PDH_HCOUNTER _PDHavailMB;

	long _privateBytes;
	long _processNonPaged;
	long _totalNonPaged;
	long _availMB;

	st_ETHERNET _EthernetStruct[e_PDH_ETHERNET_MAX];
	double _pdh_value_Network_RecvBytes;
	double _pdh_value_Network_SendBytes;


};

