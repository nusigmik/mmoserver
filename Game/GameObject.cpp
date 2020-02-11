#include "stdafx.h"
#include "GameObject.h"

GameObject::GameObject(const uuid& entity_id)
	: entity_id_(entity_id)
{
}

GameObject::~GameObject()
{
}

const uuid& GameObject::GetEntityID() const
{
	return entity_id_;
}

void GameObject::SetPosition(const Vector3 & position)
{
	position_ = position;
}

const Vector3 & GameObject::GetPosition() const
{
	return position_;
}

void GameObject::SetRotation(float rotation)
{
	rotation_ = rotation;
}

float GameObject::GetRotation() const
{
	return rotation_;
}
