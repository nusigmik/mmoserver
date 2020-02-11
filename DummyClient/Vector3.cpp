#include "stdafx.h"
#include "Vector3.h"

namespace AO
{
	namespace Vector3
	{
		//-----------------------------------------------------------------------------------
		//Fast Static members
		//-----------------------------------------------------------------------------

		const Vector3 Vector3::Zero     = Vector3(0, 0, 0);
		const Vector3 Vector3::Left     = Vector3(-1.f, 0.f, 0.f);
		const Vector3 Vector3::Right    = Vector3(1.f, 0.f, 0.f);
		const Vector3 Vector3::Up       = Vector3(0.f, 1.f, 0.f);
		const Vector3 Vector3::Down     = Vector3(0.f, -1.f, 0.f);
		const Vector3 Vector3::Forward  = Vector3(0.f, 0.f, 1.f);
		const Vector3 Vector3::Backward = Vector3(0.f, 0.f, -1.f);
	}
}