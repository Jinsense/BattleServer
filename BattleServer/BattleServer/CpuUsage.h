#ifndef _BATTLESERVER_LIB_CPU_H_
#define _BATTLESERVER_LIB_CPU_H_

/////////////////////////////
//	CCpuUsage CPUTime();
//
//	while(1)
//	{
//		CPUTime.UpdateCpuTime();
//		wprintf(L"Processor : %f / Process : %f \n", CPUTime.ProcessorTotal(), CPUTime.ProcessTotal());
//		wprintf(L"ProcessorKernel : %f / ProcessKernel : %f \n", CPUTime.ProcessorKernel(), CPUTime.ProcessKernel());
//		wprintf(L"ProcessorUser : %f / ProcessUser : %f \n", CPUTime.ProcessorKernel(), CPUTime.ProcessKernel());
//		Sleep(1000);
//	}
/////////////////////////////

class CCpuUsage
{
public:
	//	생성자, 확인대상 프로세스 핸들, 미입력시 자기 자신
	CCpuUsage(HANDLE hProcess = INVALID_HANDLE_VALUE);

	void	UpdateCpuTime();

	float	ProcessorTotal() { return _fProcessorTotal; }
	float	ProcessorUser() { return _fProcessorUser; }
	float	ProcessorKernel() { return _fProcessKernel; }

	float	ProcessTotal() { return _fProcessTotal; }
	float	ProcessUser() { return _fProcessUser; }
	float	ProcessKernel() { return _fProcessKernel; }

private:
	HANDLE	_hProcess;
	int		_iNumberOfProcessors;

	float	_fProcessorTotal;
	float	_fProcessorUser;
	float	_fProcessorKernel;

	float	_fProcessTotal;
	float	_fProcessUser;
	float	_fProcessKernel;

	ULARGE_INTEGER	_ftProcessor_LastKernel;
	ULARGE_INTEGER	_ftProcessor_LastUser;
	ULARGE_INTEGER	_ftProcessor_LastIdle;

	ULARGE_INTEGER	_ftProcess_LastKernel;
	ULARGE_INTEGER	_ftProcess_LastUser;
	ULARGE_INTEGER	_ftProcess_LastTime;
};

#endif // !_CPU_USAGE_H_
