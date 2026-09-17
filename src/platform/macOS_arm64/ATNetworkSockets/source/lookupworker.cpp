// Altirra asynchronous network lookup worker for POSIX platforms

#include <netdb.h>

#include <at/atcore/asyncdispatcher.h>
#include <at/atnetworksockets/socketutils_posix.h>
#include <at/atnetworksockets/internal/lookupworker.h>
#include <vd2/system/text.h>

ATNetLookupResult::ATNetLookupResult(ATNetSyncContext& syncContext, const wchar_t *nodename, const wchar_t *service)
	: mSyncContext(syncContext)
	, mState((int)State::Pending)
	, mNodeName(nodename)
	, mService(service)
{
}

bool ATNetLookupResult::IsAbandoned() const {
	return mRefCount == 1;
}

void ATNetLookupResult::SetAddress(const ATSocketAddress& addr) {
	vdfunction<void(const ATSocketAddress&)> fn;

	vdsynchronized(mSyncContext.mMutex) {
		mAddress = addr;
		mState = (int)State::Completed;

		if (mpOnCompleteDispatcher) {
			mpOnCompleteDispatcher->Queue(&mOnCompleteDispatcherToken,
				[self = vdrefptr(this)]() {
					vdfunction<void(const ATSocketAddress&)> fn2;

					vdsynchronized(self->mSyncContext.mMutex) {
						self->mpOnCompleteDispatcher = nullptr;
						fn2 = std::move(self->mpOnCompleteFn);
					}

					if (fn2)
						fn2(self->mAddress);
				}
			);
		} else
			fn = std::move(mpOnCompleteFn);
	}

	if (fn)
		fn(mAddress);
}

void ATNetLookupResult::SetOnCompleted(IATAsyncDispatcher *dispatcher, vdfunction<void(const ATSocketAddress&)> fn, bool callIfReady) {
	vdsynchronized(mSyncContext.mMutex) {
		if (Completed()) {
			if (callIfReady && fn)
				fn(mAddress);
		} else {
			if (mpOnCompleteDispatcher) {
				mpOnCompleteDispatcher->Cancel(&mOnCompleteDispatcherToken);
				mpOnCompleteDispatcher = nullptr;
			}

			mpOnCompleteDispatcher = dispatcher;
			mpOnCompleteFn = std::move(fn);
		}
	}
}

bool ATNetLookupResult::Completed() const {
	return (State)(int)mState == State::Completed;
}

bool ATNetLookupResult::Succeeded() const {
	return (State)(int)mState == State::Completed && mAddress.IsValid();
}

const ATSocketAddress& ATNetLookupResult::Address() const {
	return mAddress;
}

ATNetLookupWorker::ATNetLookupWorker()
	: VDThread("Net lookup worker")
{
	mpSyncContext = new ATNetSyncContext;
}

ATNetLookupWorker::~ATNetLookupWorker() {
	Shutdown();
}

bool ATNetLookupWorker::Init() {
	return ThreadStart();
}

void ATNetLookupWorker::Shutdown() {
	vdsynchronized(mMutex) {
		mbExitRequested = true;
	}

	mWakeSignal.signal();
	ThreadWait();

	while(!mPendingQueue.empty()) {
		auto *p = mPendingQueue.back();
		mPendingQueue.pop_back();
		p->Release();
	}
}

vdrefptr<IATNetLookupResult> ATNetLookupWorker::Lookup(const wchar_t *nodename, const wchar_t *service) {
	vdrefptr p(new ATNetLookupResult(*mpSyncContext, nodename, service));

	bool signal = false;
	vdsynchronized(mMutex) {
		signal = mPendingQueue.empty();
		mPendingQueue.push_back(p);
	}

	p->AddRef();
	if (signal)
		mWakeSignal.signal();

	return std::move(p);
}

void ATNetLookupWorker::ThreadRun() {
	for(;;) {
		vdrefptr<ATNetLookupResult> result;

		vdsynchronized(mMutex) {
			if (mbExitRequested)
				break;

			if (!mPendingQueue.empty()) {
				result.set(mPendingQueue.front());
				mPendingQueue.pop_front();
			}
		}

		if (!result) {
			mWakeSignal.wait();
			continue;
		}

		if (result->IsAbandoned())
			continue;

		const VDStringA nodeName = VDTextWToU8(result->mNodeName.c_str(), -1);
		const VDStringA service = VDTextWToU8(result->mService.c_str(), -1);
		addrinfo *addrResult = nullptr;
		ATSocketAddress foundAddr;

		if (!getaddrinfo(nodeName.c_str(), service.c_str(), nullptr, &addrResult) && addrResult) {
			for(addrinfo *addr = addrResult; addr; addr = addr->ai_next) {
				const ATSocketAddress candidate = ATSocketFromNativeAddress(addr->ai_addr);
				if (candidate.GetType() != ATSocketAddressType::None) {
					foundAddr = candidate;
					if (candidate.GetType() == ATSocketAddressType::IPv6)
						break;
				}
			}
		}

		if (addrResult)
			freeaddrinfo(addrResult);

		result->SetAddress(foundAddr);
	}
}
