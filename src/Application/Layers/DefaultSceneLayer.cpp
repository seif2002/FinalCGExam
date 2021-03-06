#include "DefaultSceneLayer.h"

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#include <GLM/gtc/random.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

#include <filesystem>

// Graphics
#include "Graphics/Buffers/IndexBuffer.h"
#include "Graphics/Buffers/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture2D.h"
#include "Graphics/Textures/TextureCube.h"
#include "Graphics/Textures/Texture2DArray.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"
#include "Graphics/Framebuffer.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Gameplay/Components/Light.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"
#include "Gameplay/Components/PlayerController.h"
#include "Gameplay/Components/DebugKeyHandler.h"
#include "Gameplay/Components/WinBehaviour.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

#include "Application/Application.h"
#include "Gameplay/Components/ParticleSystem.h"
#include "Graphics/Textures/Texture3D.h"
#include "Graphics/Textures/Texture1D.h"
#include "Application/Layers/ImGuiDebugLayer.h"
#include "Application/Windows/DebugWindow.h"
#include "Gameplay/Components/ShadowCamera.h"
#include "Gameplay/Components/ShipMoveBehaviour.h"

DefaultSceneLayer::DefaultSceneLayer() :
	ApplicationLayer()
{
	Name = "Default Scene";
	Overrides = AppLayerFunctions::OnAppLoad;
}

DefaultSceneLayer::~DefaultSceneLayer() = default;

void DefaultSceneLayer::OnAppLoad(const nlohmann::json& config) {
	_CreateScene();
}

void DefaultSceneLayer::_CreateScene()
{
	using namespace Gameplay;
	using namespace Gameplay::Physics;

	Application& app = Application::Get();

	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene && std::filesystem::exists("scene.json")) {
		app.LoadScene("scene.json");
	} else {
		 
		// Basic gbuffer generation with no vertex manipulation
		ShaderProgram::Sptr deferredForward = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});
		deferredForward->SetDebugName("Deferred - GBuffer Generation");  

		// Our foliage shader which manipulates the vertices of the mesh
		ShaderProgram::Sptr foliageShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/foliage.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});  
		foliageShader->SetDebugName("Foliage");   

		// This shader handles our multitexturing example
		ShaderProgram::Sptr multiTextureShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/vert_multitextured.glsl" },  
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_multitextured.glsl" }
		});
		multiTextureShader->SetDebugName("Multitexturing"); 

		// This shader handles our displacement mapping example
		ShaderProgram::Sptr displacementShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});
		displacementShader->SetDebugName("Displacement Mapping");

		// This shader handles our cel shading example
		ShaderProgram::Sptr celShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/cel_shader.glsl" }
		});
		celShader->SetDebugName("Cel Shader");


		// Load in the meshes
		MeshResource::Sptr monkeyMesh = ResourceManager::CreateAsset<MeshResource>("Monkey.obj");
		MeshResource::Sptr shipMesh   = ResourceManager::CreateAsset<MeshResource>("fenrir.obj");
		
		// Load in some textures
		Texture2D::Sptr    boxTexture   = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec      = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex    = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    leafTex      = ResourceManager::CreateAsset<Texture2D>("textures/leaves.png");
		Texture2D::Sptr    red			= ResourceManager::CreateAsset<Texture2D>("textures/red.png");
		Texture2D::Sptr    yellow		= ResourceManager::CreateAsset<Texture2D>("textures/yellow.png");
		Texture2D::Sptr    green		= ResourceManager::CreateAsset<Texture2D>("textures/green.png");
		Texture2D::Sptr    cyan			= ResourceManager::CreateAsset<Texture2D>("textures/cyan.png");
		Texture2D::Sptr    blue			= ResourceManager::CreateAsset<Texture2D>("textures/blue.png");
		Texture2D::Sptr    magenta		= ResourceManager::CreateAsset<Texture2D>("textures/magenta.png");
		leafTex->SetMinFilter(MinFilter::Nearest);
		leafTex->SetMagFilter(MagFilter::Nearest);

		// Load some images for drag n' drop
		ResourceManager::CreateAsset<Texture2D>("textures/flashlight.png");
		ResourceManager::CreateAsset<Texture2D>("textures/flashlight-2.png");
		ResourceManager::CreateAsset<Texture2D>("textures/light_projection.png");

		Texture2DArray::Sptr particleTex = ResourceManager::CreateAsset<Texture2DArray>("textures/particles.png", 2, 2);

		//DebugWindow::Sptr debugWindow = app.GetLayer<ImGuiDebugLayer>()->GetWindow<DebugWindow>();

#pragma region Basic Texture Creation
		Texture2DDescription singlePixelDescriptor;
		singlePixelDescriptor.Width = singlePixelDescriptor.Height = 1;
		singlePixelDescriptor.Format = InternalFormat::RGB8;

		float normalMapDefaultData[3] = { 0.5f, 0.5f, 1.0f };
		Texture2D::Sptr normalMapDefault = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		normalMapDefault->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, normalMapDefaultData);

		float solidGrey[3] = { 0.5f, 0.5f, 0.5f };
		float solidBlack[3] = { 0.0f, 0.0f, 0.0f };
		float solidWhite[3] = { 1.0f, 1.0f, 1.0f };

		Texture2D::Sptr solidBlackTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidBlackTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidBlack);

		Texture2D::Sptr solidGreyTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidGreyTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidGrey);

		Texture2D::Sptr solidWhiteTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidWhiteTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidWhite);

#pragma endregion 

		// Loading in a 1D LUT
		Texture1D::Sptr toonLut = ResourceManager::CreateAsset<Texture1D>("luts/toon-1D.png"); 
		toonLut->SetWrap(WrapMode::ClampToEdge);

		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		ShaderProgram::Sptr      skyboxShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" } 
		});
		  
		// Create an empty scene
		Scene::Sptr scene = std::make_shared<Scene>();  

		// Setting up our enviroment map
		scene->SetSkyboxTexture(testCubemap); 
		scene->SetSkyboxShader(skyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up 
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Loading in a color lookup table
		Texture3D::Sptr lut = ResourceManager::CreateAsset<Texture3D>("luts/cool.CUBE");   

		// Configure the color correction LUT
		scene->SetColorLUT(lut);

		// Create our materials
		// This will be our box material, with no environment reflections
		Material::Sptr boxMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			boxMaterial->Name = "Box";
			boxMaterial->Set("u_Material.AlbedoMap", boxTexture);
			boxMaterial->Set("u_Material.Shininess", 0.1f);
			boxMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr playerMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			playerMaterial->Name = "Player";
			playerMaterial->Set("u_Material.AlbedoMap", blue);
			playerMaterial->Set("u_Material.NormalMap", normalMapDefault);
			playerMaterial->Set("u_Material.Shininess", 0.8f);
		}

		// This will be the reflective material, we'll make the whole thing 50% reflective
		Material::Sptr groundMaterial = ResourceManager::CreateAsset<Material>(deferredForward); 
		{
			groundMaterial->Name = "Ground";
			groundMaterial->Set("u_Material.AlbedoMap", green);
			groundMaterial->Set("u_Material.Specular", solidBlackTex);
			groundMaterial->Set("u_Material.NormalMap", normalMapDefault);
			playerMaterial->Set("u_Material.Shininess", 0.2f);
		}

		Material::Sptr grey = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			grey->Name = "Grey";
			grey->Set("u_Material.AlbedoMap", solidGreyTex);
			grey->Set("u_Material.Specular", solidBlackTex);
			grey->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr polka = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			polka->Name = "Polka";
			polka->Set("u_Material.AlbedoMap", ResourceManager::CreateAsset<Texture2D>("textures/polka.png"));
			polka->Set("u_Material.Specular", solidBlackTex);
			polka->Set("u_Material.NormalMap", normalMapDefault);
			polka->Set("u_Material.EmissiveMap", ResourceManager::CreateAsset<Texture2D>("textures/polka.png"));
		}

		Material::Sptr whiteBrick = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			whiteBrick->Name = "White Bricks";
			whiteBrick->Set("u_Material.AlbedoMap", ResourceManager::CreateAsset<Texture2D>("textures/displacement_map.png"));
			whiteBrick->Set("u_Material.Specular", solidGrey);
			whiteBrick->Set("u_Material.NormalMap", ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png"));
		}

		Material::Sptr normalmapMat = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			Texture2D::Sptr normalMap       = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap      = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			normalmapMat->Name = "Tangent Space Normal Map";
			normalmapMat->Set("u_Material.AlbedoMap", diffuseMap);
			normalmapMat->Set("u_Material.NormalMap", normalMap);
			normalmapMat->Set("u_Material.Shininess", 0.5f);
			normalmapMat->Set("u_Scale", 0.1f);
		}

		Material::Sptr multiTextureMat = ResourceManager::CreateAsset<Material>(multiTextureShader);
		{
			Texture2D::Sptr sand  = ResourceManager::CreateAsset<Texture2D>("textures/terrain/sand.png");
			Texture2D::Sptr grass = ResourceManager::CreateAsset<Texture2D>("textures/terrain/grass.png");

			multiTextureMat->Name = "Multitexturing";
			multiTextureMat->Set("u_Material.DiffuseA", sand);
			multiTextureMat->Set("u_Material.DiffuseB", grass);
			multiTextureMat->Set("u_Material.NormalMapA", normalMapDefault);
			multiTextureMat->Set("u_Material.NormalMapB", normalMapDefault);
			multiTextureMat->Set("u_Material.Shininess", 0.5f);
			multiTextureMat->Set("u_Scale", 0.1f); 
		}

		// Create some lights for our scene
		GameObject::Sptr lightParent = scene->CreateGameObject("Lights");

		for (int ix = 0; ix < 50; ix++) {
			GameObject::Sptr light = scene->CreateGameObject("Light");
			light->SetPostion(glm::vec3(glm::diskRand(25.0f), 1.0f));
			lightParent->AddChild(light);

			Light::Sptr lightComponent = light->Add<Light>();
			lightComponent->SetColor(glm::linearRand(glm::vec3(0.0f), glm::vec3(1.0f)));
			lightComponent->SetRadius(glm::linearRand(0.1f, 10.0f));
			lightComponent->SetIntensity(glm::linearRand(1.0f, 2.0f));
		}

		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
		sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 5));
		sphere->GenerateMesh();

		// Set up the scene's camera
		GameObject::Sptr camera = scene->MainCamera->GetGameObject()->SelfRef();
		{
			camera->SetPostion({ -5, 0, 2 });
			camera->LookAt(glm::vec3(0.0f, 0.0f, 2.0f));
			scene->MainCamera->SetFovDegrees(70.0f);

			
			DebugKeyHandler::Sptr handler = camera->Add<DebugKeyHandler>();
			// This is now handled by scene itself!
			//Camera::Sptr cam = camera->Add<Camera>();
			// Make sure that the camera is set as the scene's main camera!
			//scene->MainCamera = cam;
		}
		// Player object
		GameObject::Sptr player = scene->CreateGameObject("Player");
		{
			MeshResource::Sptr box = ResourceManager::CreateAsset<MeshResource>();
			box->AddParam(MeshBuilderParam::CreateCube(glm::vec3(0, 0, 0.1f), ONE));
			box->GenerateMesh();
			 
			// Set and rotation position in the scene
			player->SetPostion(glm::vec3(0.0f, 4.5f, 2.5f));
			player->SetScale(glm::vec3(0.5f, 0.5f, 0.5f));

			// Add a render component
			RenderComponent::Sptr renderer = player->Add<RenderComponent>();
			renderer->SetMesh(box);
			renderer->SetMaterial(playerMaterial);

			RigidBody::Sptr playerHitbox = player->Add<RigidBody>(RigidBodyType::Dynamic);
			playerHitbox->AddCollider(BoxCollider::Create(glm::vec3(0.25f, 0.25f, 0.25f)))->SetPosition({ 0,0,0.110 });

			PlayerController::Sptr playerController = player->Add<PlayerController>();

			ParticleSystem::Sptr particleManager = player->Add<ParticleSystem>();
			particleManager->Atlas = particleTex;

			particleManager->_gravity = glm::vec3(0.0f);

			ParticleSystem::ParticleData emitter;
			emitter.Type = ParticleType::SphereEmitter;
			emitter.TexID = 3;
			emitter.Position = glm::vec3(0.0f, 0.0f, 0.0f);
			emitter.Color = glm::vec4(0.235f, 0.235f, 1.0f, 0.7f);
			emitter.Lifetime = 1.0f / 50.0f;
			emitter.SphereEmitterData.Timer = 1.0f / 50.0f;
			emitter.SphereEmitterData.Velocity = 0.2f;
			emitter.SphereEmitterData.LifeRange = { 1.0f, 1.0f };
			emitter.SphereEmitterData.Radius = 0.1f;
			emitter.SphereEmitterData.SizeRange = { 0.5f, 0.5f };

			particleManager->AddEmitter(emitter);
		}
		 
		// Set up all our sample objects
		GameObject::Sptr plane = scene->CreateGameObject("Plane");
		{
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(groundMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });

			TriggerVolume::Sptr trigger = plane->Add<TriggerVolume>();
			BoxCollider::Sptr collider = BoxCollider::Create(glm::vec3(0.5f));
			collider->SetPosition(glm::vec3(0.0f, 0.0f, 0.10f));
			collider->SetScale(glm::vec3(1.0f, 15.0f, 1.0f));
			trigger->AddCollider(collider);

			TriggerVolumeEnterBehaviour::Sptr volume = plane->Add<TriggerVolumeEnterBehaviour>();
			volume->playerController = player->Get<PlayerController>();

			ParticleSystem::Sptr particleManager = plane->Add<ParticleSystem>();
			particleManager->Atlas = particleTex;

			particleManager->_gravity = glm::vec3(0.0f, 0.0f, 0.5f);

			ParticleSystem::ParticleData emitter;
			emitter.Type = ParticleType::BoxEmitter;
			emitter.TexID = 1;
			emitter.Position = glm::vec3(3.0f, 0.0f, 0.0f);
			emitter.Color = glm::vec4(0.789f, 0.239f, 0.162f, 0.718f);
			emitter.Lifetime = 1.0f / 50.0f;
			emitter.BoxEmitterData.Timer = 1.0f / 5.0f;
			emitter.BoxEmitterData.LifeRange = { 5.0f, 10.0f };
			emitter.BoxEmitterData.SizeRange = { 0.15f,0.3f };
			emitter.BoxEmitterData.Velocity = glm::vec3(0.0f,0.0f, 3.0f);
			emitter.BoxEmitterData.HalfExtents = glm::vec3(1.0f, 6.0f, 0.05f);

			particleManager->AddEmitter(emitter);
		}

		

		// Add some walls :3
		{
			MeshResource::Sptr wall = ResourceManager::CreateAsset<MeshResource>();
			wall->AddParam(MeshBuilderParam::CreateCube(ZERO, ONE));
			wall->GenerateMesh();

			GameObject::Sptr wall1 = scene->CreateGameObject("Wall1");
			wall1->Add<RenderComponent>()->SetMesh(wall)->SetMaterial(groundMaterial);
			wall1->SetScale(glm::vec3(2.0f, 2.0f, 0.5f));
			wall1->SetPostion(glm::vec3(0.0f, 4.5f, 1.5f));
			
			RigidBody::Sptr physics1 = wall1->Add<RigidBody>(/*static by default*/);
			physics1->AddCollider(BoxCollider::Create(glm::vec3(1.0f, 1.0f, 0.25f)))->SetPosition({ 0,0,0});
			
			TriggerVolume::Sptr trigger1 = wall1->Add<TriggerVolume>();
			BoxCollider::Sptr collider1 = BoxCollider::Create(glm::vec3(0.5f));
			collider1->SetPosition(glm::vec3(0.0f, 0.0f, 0.15f));
			collider1->SetScale(glm::vec3(2.0f, 2.0f, 0.2f));
			trigger1->AddCollider(collider1);

			TriggerVolumeEnterBehaviour::Sptr volume1 = wall1->Add<TriggerVolumeEnterBehaviour>();
			volume1->playerController = player->Get<PlayerController>();
			
			plane->AddChild(wall1);


			GameObject::Sptr wall2 = scene->CreateGameObject("Wall2");
			wall2->Add<RenderComponent>()->SetMesh(wall)->SetMaterial(groundMaterial);
			wall2->SetScale(glm::vec3(2.0f, 2.0f, 0.5f));
			wall2->SetPostion(glm::vec3(0.0f, -4.5f, 1.5f));

			RigidBody::Sptr physics2 = wall2->Add<RigidBody>(/*static by default*/);
			physics2->AddCollider(BoxCollider::Create(glm::vec3(1.0f, 1.0f, 0.25f)))->SetPosition({ 0,0,0 });
			
			TriggerVolume::Sptr trigger2 = wall2->Add<TriggerVolume>();
			BoxCollider::Sptr collider2 = BoxCollider::Create(glm::vec3(0.5f));
			collider2->SetPosition(glm::vec3(0.0f, 0.0f, 0.15f));
			collider2->SetScale(glm::vec3(2.0f, 2.0f, 0.2f));
			trigger2->AddCollider(collider2);

			TriggerVolumeEnterBehaviour::Sptr volume2 = wall2->Add<TriggerVolumeEnterBehaviour>();
			volume2->playerController = player->Get<PlayerController>();
			
			plane->AddChild(wall2);

			GameObject::Sptr wall3 = scene->CreateGameObject("Wall3");
			wall3->Add<RenderComponent>()->SetMesh(wall)->SetMaterial(groundMaterial);
			wall3->SetScale(glm::vec3(1.0f, 20.0f, 3.0f));
			wall3->SetPostion(glm::vec3(10.0f, 0.0f, 1.5f));
			plane->AddChild(wall3);

			GameObject::Sptr wall4 = scene->CreateGameObject("Wall4");
			wall4->Add<RenderComponent>()->SetMesh(wall)->SetMaterial(groundMaterial);
			wall4->SetScale(glm::vec3(1.0f, 20.0f, 3.0f));
			wall4->SetPostion(glm::vec3(-10.0f, 0.0f, 1.5f));
			plane->AddChild(wall4);
		}

		GameObject::Sptr egg = scene->CreateGameObject("Egg");
		{
			egg->SetPostion(glm::vec3(0, 0, 4));
			egg->SetScale(glm::vec3(0.5f));
			 
			RenderComponent::Sptr renderer = egg->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(grey);

			RotatingBehaviour::Sptr move = egg->Add<RotatingBehaviour>();
			move->RotationSpeed = glm::vec3(0, 0, 180);
			move->MoveSpeed = glm::vec3(0, 1, 0.25f);

			TriggerVolume::Sptr trigger = egg->Add<TriggerVolume>();
			SphereCollider::Sptr collider = SphereCollider::Create(0.5f);
			collider->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
			trigger->AddCollider(collider);
			 
			SphereCollider::Sptr collider2 = SphereCollider::Create(0.45f);
			collider2->SetPosition(glm::vec3(0.0f, 0.0f, -3));
			trigger->AddCollider(collider2);

			ParticleSystem::Sptr particleManager = egg->Add<ParticleSystem>();
			particleManager->Atlas = particleTex;

			particleManager->_gravity = glm::vec3(0.0f, 0.0f, 0.3f);

			ParticleSystem::ParticleData emitter;
			emitter.Type = ParticleType::SphereEmitter;
			emitter.TexID = 2;
			emitter.Position = glm::vec3(0.0f, 0.0f, -0.3f);
			emitter.Color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
			emitter.Lifetime = 1.0f / 50.0f;
			emitter.SphereEmitterData.Timer = 1.0f / 50.0f;
			emitter.SphereEmitterData.Velocity = 0.5f;
			emitter.SphereEmitterData.LifeRange = { 1.0f, 3.0f };
			emitter.SphereEmitterData.Radius = 0.1f;
			emitter.SphereEmitterData.SizeRange = { 0.1f, 0.5f };

			particleManager->AddEmitter(emitter);
		}

		GameObject::Sptr ball = scene->CreateGameObject("Ball");
		{
			ball->SetPostion(glm::vec3(0, 0, -6));
			ball->SetScale(glm::vec3(0.9f));

			RenderComponent::Sptr renderer = ball ->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(grey);



			egg->AddChild(ball);
		}

		GameObject::Sptr win = scene->CreateGameObject("Win");
		{
			win->SetPostion(glm::vec3(0.0f, -4.5f, 1.5f));

			TriggerVolume::Sptr trigger = win->Add<TriggerVolume>();
			BoxCollider::Sptr collider = BoxCollider::Create(glm::vec3(0.5f));
			collider->SetPosition(glm::vec3(0.0f, 0.0f, 0.15f));
			collider->SetScale(glm::vec3(2.0f, 2.0f, 0.2f));
			trigger->AddCollider(collider);

			WinBehaviour::Sptr winner = win->Add<WinBehaviour>();
		}

		//GameObject::Sptr ship = scene->CreateGameObject("Fenrir");
		//{
		//	// Set position in the scene
		//	ship->SetPostion(glm::vec3(1.5f, 0.0f, 4.0f));
		//	ship->SetScale(glm::vec3(0.1f));

		//	// Create and attach a renderer for the monkey
		//	RenderComponent::Sptr renderer = ship->Add<RenderComponent>();
		//	renderer->SetMesh(shipMesh);
		//	renderer->SetMaterial(grey);

		//	GameObject::Sptr particles = scene->CreateGameObject("Particles");
		//	ship->AddChild(particles);
		//	particles->SetPostion({ 0.0f, -7.0f, 0.0f});

		//	ParticleSystem::Sptr particleManager = particles->Add<ParticleSystem>();
		//	particleManager->Atlas = particleTex;

		//	particleManager->_gravity = glm::vec3(0.0f);

		//	ParticleSystem::ParticleData emitter;
		//	emitter.Type = ParticleType::SphereEmitter;
		//	emitter.TexID = 2;
		//	emitter.Position = glm::vec3(0.0f);
		//	emitter.Color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
		//	emitter.Lifetime = 1.0f / 50.0f;
		//	emitter.SphereEmitterData.Timer = 1.0f / 50.0f;
		//	emitter.SphereEmitterData.Velocity = 0.5f;
		//	emitter.SphereEmitterData.LifeRange = { 1.0f, 3.0f };
		//	emitter.SphereEmitterData.Radius = 0.5f;
		//	emitter.SphereEmitterData.SizeRange = { 0.5f, 1.0f };

		//	ParticleSystem::ParticleData emitter2;
		//	emitter2.Type = ParticleType::SphereEmitter;
		//	emitter2.TexID = 2;
		//	emitter2.Position = glm::vec3(0.0f);
		//	emitter2.Color = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
		//	emitter2.Lifetime = 1.0f / 40.0f;
		//	emitter2.SphereEmitterData.Timer = 1.0f / 40.0f;
		//	emitter2.SphereEmitterData.Velocity = 0.1f;
		//	emitter2.SphereEmitterData.LifeRange = { 0.5f, 1.0f };
		//	emitter2.SphereEmitterData.Radius = 0.25f;
		//	emitter2.SphereEmitterData.SizeRange = { 0.25f, 0.5f };

		//	particleManager->AddEmitter(emitter);
		//	particleManager->AddEmitter(emitter2);

		//	ShipMoveBehaviour::Sptr move = ship->Add<ShipMoveBehaviour>();
		//	move->Center = glm::vec3(0.0f, 0.0f, 4.0f);
		//	move->Speed = 180.0f;
		//	move->Radius = 6.0f;
		//}

		GameObject::Sptr demoBase = scene->CreateGameObject("Demo Parent");
		

		GameObject::Sptr shadowCaster = scene->CreateGameObject("Shadow Light");
		{
			// Set position in the scene
			shadowCaster->SetPostion(glm::vec3(0.0f, 0.01f, 15.0f));
			shadowCaster->LookAt(glm::vec3(0.0f));

			// Create and attach a renderer for the monkey
			ShadowCamera::Sptr shadowCam = shadowCaster->Add<ShadowCamera>();
			shadowCam->SetProjection(glm::perspective(glm::radians(120.0f), 1.0f, 0.1f, 100.0f));
		}

		/////////////////////////// UI //////////////////////////////
		
		
		

		/*GameObject::Sptr particles = scene->CreateGameObject("Particles"); 
		{
			particles->SetPostion({ -2.0f, 0.0f, 2.0f });

			ParticleSystem::Sptr particleManager = particles->Add<ParticleSystem>();  
			particleManager->Atlas = particleTex;

			ParticleSystem::ParticleData emitter;
			emitter.Type = ParticleType::SphereEmitter;
			emitter.TexID = 2;
			emitter.Position = glm::vec3(0.0f);
			emitter.Color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
			emitter.Lifetime = 0.0f;
			emitter.SphereEmitterData.Timer = 1.0f / 50.0f;
			emitter.SphereEmitterData.Velocity = 0.5f;
			emitter.SphereEmitterData.LifeRange = { 1.0f, 4.0f };
			emitter.SphereEmitterData.Radius = 1.0f;
			emitter.SphereEmitterData.SizeRange = { 0.5f, 1.5f };

			particleManager->AddEmitter(emitter);
		}*/

		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("scene-manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");

		// Send the scene to the application
		app.LoadScene(scene);
	}
}
