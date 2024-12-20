#pragma once
#include"pch.h"
#include<atomic>
#include<list>
#include "Thread.h"
template<class T>
class CQueue
{//线程安全的队列（利用IOCP实现）
public:
	enum {
		CQNone,
		CQPush,
		CQPop,
		CQSize,
		CQClear
	};
	typedef struct IocpParam {
		size_t nOperator;//操作
		T Data;//数据
		HANDLE hEvent;//pop操作需要的
		IocpParam(int op, const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = CQNone;
		}
	}PPARAM;//Post Parameter 用于投递信息的结构体
public:
	CQueue() {
		m_lock = false;
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompletionPort != NULL) {
			m_hThread = (HANDLE)_beginthread(&CQueue<T>::threadEntry, 0, this);
		}
	}
	virtual ~CQueue() {
		if (m_lock) return;
		m_lock = true;	
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
		WaitForSingleObject(m_hThread, INFINITE);
		if (m_hCompletionPort != NULL) {
			HANDLE hTemp = m_hCompletionPort;
			m_hCompletionPort = NULL;
			CloseHandle(hTemp);
		}
	}
	bool PushBack(const T& data) {
		IocpParam* pParam = new IocpParam(CQPush, data);
		if (m_lock == true) 
		{
			delete pParam;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false)delete pParam;
		//printf("push back done %d %08p\r\n", ret,(void*)pParam);
		return ret;
	}
	virtual bool PopFront(T& data) {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(CQPop, data, hEvent);
		if (m_lock)
		{
			if (hEvent) CloseHandle(hEvent);
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == false)
		{
			CloseHandle(hEvent);
			return false;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			data = Param.Data;
		}
		return ret;
	}
	size_t Size() {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IocpParam Param(CQSize, T(), hEvent);
		if (m_lock)
		{
			if (hEvent) CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == false)
		{
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			return Param.nOperator;
		}
		return -1;
	}
	bool Clear() {
		if (m_lock == true) return false;
		IocpParam* pParam = new IocpParam(CQClear, T());
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false)delete pParam;
		//printf("push back done %d %08p\r\n", ret, (void*)pParam);
		return ret;
	}
protected:
	static void threadEntry(void* arg) {
		CQueue<T>* thiz = (CQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}
	virtual void DealParam(PPARAM* pParam) {
		switch (pParam->nOperator) {
		case CQPush:
			m_lstData.push_back(pParam->Data);
			delete pParam;
			//printf("delete %08p\r\n",(void*)pParam);
			break;
		case CQPop:
			if (m_lstData.size() > 0) {
				pParam->Data = m_lstData.front();
				m_lstData.pop_front();
			}
			if (pParam->hEvent != NULL) {
				SetEvent(pParam->hEvent);
			}
			break;
		case CQSize:
			pParam->nOperator = m_lstData.size();
			if (pParam->hEvent != NULL) {
				SetEvent(pParam->hEvent);
			}
			break;
		case CQClear:
			m_lstData.clear();
			delete pParam;
			//printf("delete %08p\r\n", (void*)pParam);
			break;
		default:
			OutputDebugStringA("unknown operator!\r\n");
			break;
		}
	}
	void threadMain() {
		PPARAM* pParam = NULL;
		DWORD dwTransferred = 0;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		while (GetQueuedCompletionStatus(m_hCompletionPort, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE)) {
			if (dwTransferred == 0 || CompletionKey == NULL) {
				printf("thread is prepare to exit!\r\n");
				break;
			}
			pParam = (PPARAM*)CompletionKey;
			DealParam(pParam);
		}
		while (GetQueuedCompletionStatus(m_hCompletionPort, &dwTransferred, &CompletionKey, &pOverlapped, 0)) {
			if (dwTransferred == 0 || CompletionKey == NULL) {
				printf("thread is prepare to exit!\r\n");
				continue;
			}
			pParam = (PPARAM*)CompletionKey;
			DealParam(pParam);
		}
		HANDLE hTemp = m_hCompletionPort;
		m_hCompletionPort = NULL;
		CloseHandle(hTemp);	
	}
protected:
	std::list<T> m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
	std::atomic<bool> m_lock;//队列正在析构
};

template<class T>
class CSendQueue : public CQueue<T> ,public ThreadFuncBase{
public:
	typedef int (ThreadFuncBase::* ECALLBACK)(T& data);
	CSendQueue(ThreadFuncBase* obj, ECALLBACK callback) : CQueue<T>(),m_base(obj),m_callback(callback){
		m_thread.Start();
		m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE) & CSendQueue<T>::threadTick));
	}
	virtual ~CSendQueue(){
		m_base = NULL;
		m_callback = NULL;
		m_thread.Stop();
	}
protected:
	virtual bool PopFront(T& data) { return false; }
	bool PopFront() {
		typename CQueue<T>::IocpParam* Param = new typename CQueue<T>::IocpParam(CQueue<T>::CQPop, T());
		if (CQueue<T>::m_lock)
		{
			delete Param;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(CQueue<T>::m_hCompletionPort, sizeof(*Param), (ULONG_PTR)&Param, NULL);
		if (ret == false)
		{
			delete Param;
			return false;
		}
		return ret;
	}
	int threadTick() {
		if (WaitForSingleObject(CQueue<T>::m_hThread,0) != WAIT_TIMEOUT) return 0;
		if (CQueue<T>::m_lstData.size() > 0) {
			PopFront();
		}
		return 0;
	}
	virtual void DealParam(typename CQueue<T>::PPARAM* pParam) {
		switch (pParam->nOperator) {
		case CQueue<T>::CQPush:
			CQueue<T>::m_lstData.push_back(pParam->Data);
			delete pParam;
			//printf("delete %08p\r\n",(void*)pParam);
			break;
		case CQueue<T>::CQPop:
			if (CQueue<T>::m_lstData.size() > 0) {
				pParam->Data = CQueue<T>::m_lstData.front();
				if((m_base->*m_callback)(pParam->Data)==0)
				{
					CQueue<T>::m_lstData.pop_front();
				}
			}
			delete pParam;
			break;
		case CQueue<T>::CQSize:
			pParam->nOperator = CQueue<T>::m_lstData.size();
			if (pParam->hEvent != NULL) {
				SetEvent(pParam->hEvent);
			}
			break;
		case CQueue<T>::CQClear:
			CQueue<T>::m_lstData.clear();
			delete pParam;
			//printf("delete %08p\r\n", (void*)pParam);
			break;
		default:
			OutputDebugStringA("unknown operator!\r\n");
			break;
		}
	}
private:
	ThreadFuncBase* m_base;
	ECALLBACK m_callback;
	CThread m_thread;
};
typedef CSendQueue<std::vector<char>>::ECALLBACK SENDCALLBACK;