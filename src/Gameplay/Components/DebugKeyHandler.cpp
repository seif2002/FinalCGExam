#pragma once
#include "DebugKeyHandler.h"
#include <GLFW/glfw3.h>

#include "Utils/ImGuiHelper.h"
#include "Utils/JsonGlmHelpers.h"

// GetGameObject()->GetRotationEuler();

DebugKeyHandler::DebugKeyHandler() :
	IComponent(),
	_renderer(nullptr),
	diffuse(true),
	ambient(true),
	specular(true),
	post1(false),
	post2(false),
	post3(false)
{}

void DebugKeyHandler::Awake() {
	_renderer = GetComponent<RenderComponent>();
}

void DebugKeyHandler::Update(float deltaTime) {

	// Lighting Keys
	if (InputEngine::GetKeyState(GLFW_KEY_1) == ButtonState::Pressed) // diffuse toggle
	{
		if (diffuse)
		{
			diffuse = false;
		}
		else
		{
			diffuse = true;
		}
	}
	if (InputEngine::GetKeyState(GLFW_KEY_2) == ButtonState::Pressed) // ambient toggle
	{
		if (ambient)
		{
			
			ambient = false;
		}
		else
		{
			
			ambient = true;
		}
	}
	if (InputEngine::GetKeyState(GLFW_KEY_3) == ButtonState::Pressed) // specular toggle
	{
		if (specular)
		{
			
			specular = false;
		}
		else
		{
			
			specular = true;
		}
	}
	if (InputEngine::GetKeyState(GLFW_KEY_4) == ButtonState::Pressed) // post-processing toggle #1
	{
		if (post1)
		{
			postProcess->GetEffect<Pixelation>()->Enabled = false;
			post1 = false;
		}
		else
		{
			postProcess->GetEffect<Pixelation>()->Enabled = true;
			post1 = true;
		}
	}
	if (InputEngine::GetKeyState(GLFW_KEY_5) == ButtonState::Pressed) // post-processing toggle #2
	{
		if (post2)
		{
			postProcess->GetEffect<Filmgrain>()->Enabled = false;
			post2 = false;
		}
		else
		{
			postProcess->GetEffect<Filmgrain>()->Enabled = true;
			post2 = true;
		}
	}
	if (InputEngine::GetKeyState(GLFW_KEY_6) == ButtonState::Pressed) // post-processing toggle #3
	{
		if (post3)
		{
			postProcess->GetEffect<ColorCorrectionEffect>()->Enabled = false;
			postProcess->GetEffect<Toonshading>()->Enabled = false;
			postProcess->GetEffect<Nightvision>()->Enabled = false;
			postProcess->GetEffect<OutlineEffect>()->Enabled = false;
			post3 = false;
		}
		else
		{
			postProcess->GetEffect<ColorCorrectionEffect>()->Enabled = true;
			postProcess->GetEffect<Toonshading>()->Enabled = true;
			postProcess->GetEffect<Nightvision>()->Enabled = true;
			postProcess->GetEffect<OutlineEffect>()->Enabled = true;
			post3 = true;
		}
	}

	//	Reset Key
	if (InputEngine::GetKeyState(GLFW_KEY_R) == ButtonState::Pressed)
	{
		ambient = true;
		diffuse = true;
		specular = true;
	}
}

void DebugKeyHandler::RenderImGui() {

}

nlohmann::json DebugKeyHandler::ToJson() const {
	return {
		{ "diffuse", diffuse }
	};
}

DebugKeyHandler::Sptr DebugKeyHandler::FromJson(const nlohmann::json& data) {
	DebugKeyHandler::Sptr result = std::make_shared<DebugKeyHandler>();
	result->diffuse = JsonGet(data, "diffuse", result->diffuse);
	return result;
}