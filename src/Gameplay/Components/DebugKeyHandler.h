#pragma once
#include "Gameplay/Components/IComponent.h"
#include <math.h>

#include "Gameplay/GameObject.h"
#include "RenderComponent.h"
#include "Gameplay/Scene.h"
#include "Gameplay/InputEngine.h"
#include "Gameplay/Material.h"

#include "Application/Layers/PostProcessingLayer.h"
#include "Application/Application.h"


#include "Application/Layers/PostProcessing/Pixelation.h"
#include "Application/Layers/PostProcessing/ColorCorrectionEffect.h"
#include "Application/Layers/PostProcessing/BoxFilter3x3.h"
#include "Application/Layers/PostProcessing/BoxFilter5x5.h"
#include "Application/Layers/PostProcessing/OutlineEffect.h"
#include "Application/Layers/PostProcessing/DepthOfField.h"
#include "Application/Layers/PostProcessing/Nightvision.h"
#include "Application/Layers/PostProcessing/Filmgrain.h"
#include "Application/Layers/PostProcessing/Toonshading.h"

/// <summary>
/// Showcases a very simple behaviour that rotates the parent gameobject at a fixed rate over time
/// </summary>
class DebugKeyHandler : public Gameplay::IComponent {
public:
	typedef std::shared_ptr<DebugKeyHandler> Sptr;
	
	RenderComponent::Sptr _renderer;
	bool diffuse;
	bool ambient;
	bool specular;
	bool post1;
	bool post2;
	bool post3;

	PostProcessingLayer::Sptr& postProcess = Application::Get().GetLayer<PostProcessingLayer>();

	DebugKeyHandler();

	virtual void Awake() override;
	virtual void Update(float deltaTime) override;

	virtual void RenderImGui() override;

	virtual nlohmann::json ToJson() const override;
	static DebugKeyHandler::Sptr FromJson(const nlohmann::json& data);

	MAKE_TYPENAME(DebugKeyHandler);
};