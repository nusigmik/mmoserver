#pragma once
#include "Common.h"
#include "Vector3.h"

using AO::Vector3::Vector3;

class GameObject
{
public:
	GameObject(const uuid& entity_id);
	virtual ~GameObject();

	virtual void Update(double delta_time) = 0;

	const uuid& GetEntityID() const;
	void SetPosition(const Vector3& position);
	const Vector3& GetPosition() const;
	void SetRotation(float rotation);
	float GetRotation() const;

private:
	uuid entity_id_;
	Vector3 position_;
	float rotation_;
};
