// Windows input-manager bindings for the shared controller implementations.
#include <stdafx.h>
#include "inputcontroller.h"
#include "inputmanager.h"

ATConsoleController::ATConsoleController(ATInputManager *im)
	: ATConsoleController([im](uint32 trigger, bool state) {
		if (auto *callback = im->GetConsoleCallback())
			callback->SetConsoleTrigger(trigger, state);
	})
{
}

ATInputStateController::ATInputStateController(ATInputManager *im, uint32 flagBase)
	: ATInputStateController([im](uint32 flag, bool state) { im->ActivateFlag(flag, state); }, flagBase)
{
}
