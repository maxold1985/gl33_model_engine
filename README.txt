OPENGL 3.3 GLB ENGINE
======================

Alvo:
- Windows 7 SP1
- WinLibs / MinGW-w64
- Intel HD 4000 ou GPU com OpenGL 3.3+
- OpenGL 3.3 Core
- Sem GLFW / GLAD / GLEW

RECURSOS
========
- leitor GLB 2.0 proprio
- POSITION
- NORMAL opcional
- TEXCOORD_0 opcional
- indices UNSIGNED_BYTE / UNSIGNED_SHORT / UNSIGNED_INT
- baseColorFactor
- baseColorTexture
- PNG/JPEG embutido no GLB
- PNG/JPEG externo relativo ao GLB
- decoder de imagem pelo Windows Imaging Component (WIC)
- VAO/VBO/EBO
- GLSL 330
- depth test
- iluminacao simples
- menu desenhado em OpenGL

MENU / CONTROLES
================
F1 ou clique OPEN GLB
    abre o seletor de arquivo do Windows.

F2 ou clique TEXTURE
    liga/desliga a textura.

F3 ou clique WIREFRAME
    liga/desliga wireframe.

F4 ou clique CUBE MODEL
    alterna entre cubo interno e GLB carregado.

ESC
    fecha.

ESTRUTURA
=========
src/main.cpp
src/engine.cpp
src/engine.h
src/gl33.cpp
src/gl33.h
src/math3d.cpp
src/math3d.h
src/glb_loader.cpp
src/glb_loader.h
src/ui.cpp
src/ui.h
src/renderer.cpp
src/renderer.h

assets/sample.glb
    GLB simples com textura embutida para teste.

LIMITES DESTA PRIMEIRA VERSAO
=============================
- carrega o primeiro mesh e a primeira primitive do GLB.
- nao aplica animacao, skinning ou morph targets.
- nao aplica arvore de nodes/transforms do glTF.
- material implementado: base color.
- Draco e KTX2/BasisU nao sao suportados.
- o ideal ao exportar do Blender e usar GLB normal, textura PNG/JPEG,
  sem Draco.

COMPILAR
========
Execute:
    build.bat

O executavel sera:
    build\glb_engine.exe


ATUALIZACAO DE MATERIAIS
========================
Esta versao agora:
- renderiza TODAS as primitives de todos os meshes do GLB;
- cada primitive usa o seu proprio material;
- suporta baseColorTexture;
- suporta baseColorFactor;
- usa metallicFactor e roughnessFactor em iluminacao aproximada;
- mostra no console quantas primitives, materials e textures foram carregadas.

Isso corrige GLBs do Blender que usam varios Material Slots, pois o Blender
normalmente separa esses materiais em primitives diferentes dentro do glTF.


SUPORTE FBX
===========
A engine agora abre:
- .glb
- .fbx

GLB:
- loader proprio da engine.

FBX:
- importado via Assimp.
- suporta FBX binario e ASCII conforme suporte do Assimp.
- meshes multiplos.
- triangulacao.
- normals.
- UV0.
- transforms de nodes aplicados com aiProcess_PreTransformVertices.
- materiais diffuse/base color.
- opacity.
- metallic/roughness quando o Assimp fornece esses valores.
- textura diffuse/base color externa.
- textura embutida suportada pelo Assimp.
- PNG/JPEG decodificados pelo Windows Imaging Component.

F1 agora abre GLB ou FBX.

ATENCAO 32/64 BIT
=================
O compilador e a biblioteca Assimp precisam ter a mesma arquitetura.

Exemplo:
- GCC i686 -> Assimp i686.
- GCC x86_64 -> Assimp x86_64.

Veja setup_assimp.txt.


FBX DEBUG
=========
Ao carregar um FBX, o console agora mostra:

    Selected file: F:\...\modelo.fbx
    Loading model: F:\...\modelo.fbx
    FBX Assimp: meshes=... materials=... textures=... animations=...

Se falhar:

    MODEL LOAD ERROR:
    Assimp FBX error: ...

A importacao agora tenta duas configuracoes:
1. importacao completa com PreTransformVertices;
2. fallback mais simples se a primeira falhar.

Tambem foi removido aiProcess_ValidateDataStructure do caminho normal,
pois ele pode rejeitar FBX que ainda possuem meshes utilizaveis.

Se aparecer:
    Assimp was built without FBX importer support

a biblioteca Assimp usada foi compilada sem o importador FBX.


FBX ANIMATION READER
====================
O loader FBX agora preserva e le:

- arvore de nodes;
- transform local de cada node;
- bones por mesh;
- offset matrix de cada bone;
- pesos bone -> vertex;
- todos os clips aiAnimation;
- duration em ticks;
- ticksPerSecond;
- duration em segundos;
- aiNodeAnim;
- position keys;
- quaternion rotation keys;
- scaling keys.

Dados retornados em:

    GlbModel model;

    model.nodes
    model.animations
    model.primitives[i].bones

Exemplo:

    for (const ModelAnimationClip& clip : model.animations) {
        printf("%s %.3f sec\n",
            clip.name.c_str(),
            clip.duration_seconds);

        for (const ModelAnimationChannel& channel : clip.channels) {
            printf("node: %s pos=%u rot=%u scale=%u\n",
                channel.node_name.c_str(),
                (unsigned)channel.position_keys.size(),
                (unsigned)channel.rotation_keys.size(),
                (unsigned)channel.scaling_keys.size());
        }
    }

IMPORTANTE
==========
Esta etapa IMPLEMENTA A LEITURA dos dados de animacao e skeleton.

O renderer atual ainda mostra a pose estatica e nao aplica skinning GPU
por frame. Para reproduzir a animacao visualmente ainda falta o modulo:
AnimationPlayer -> interpolacao de keys -> hierarquia -> bone matrices ->
skin de vertices no shader.

DLLs
====
Depois de compilar a engine, build.bat agora copia TODAS as DLLs de:

    %ASSIMP_ROOT%\bin\*.dll

para:

    build\

Tambem copia do WinLibs:

    libstdc++-6.dll
    libgcc_s_dw2-1.dll
    libwinpthread-1.dll

Assim model_engine.exe fica com as DLLs necessarias ao lado dele.


WIN32 MENUS + EDIT BOX
======================

Agora a engine usa controles nativos do Windows 7.

Menu File:
    Open Model...
    Reload
    Exit

Menu View:
    Texture
    Wireframe
    Show Model

Menu Animation:
    Play / Pause
    Previous Clip
    Next Clip

Menu Help:
    About

Edit Boxes:
    Model
        caminho do GLB/FBX

    Anim
        indice do clip de animacao

    Speed
        velocidade da animacao
        intervalo aplicado: 0.0 ate 10.0

    Time
        tempo atual em segundos

Botao:
    Apply

O build.bat continua copiando as DLLs do Assimp e do runtime MinGW
para a pasta build, ao lado de model_engine.exe.

OBSERVACAO
==========
O leitor FBX ja le clips, hierarchy, bones e keyframes.
Os menus/edit boxes controlam o estado de animacao.

O skinning visual por bones no shader ainda nao foi implementado nesta etapa.


FIX: ANIMATION RUNNING NAO TOCAVA
=================================

A versao anterior apenas:
    animation_time += dt

e lia os keyframes, mas nao aplicava a pose aos vertices.

Agora o pipeline e:

    animation_time
        |
        v
    seconds -> Assimp ticks
        |
        v
    position key interpolation
    quaternion SLERP
    scale interpolation
        |
        v
    local node matrices
        |
        v
    parent * child hierarchy
        |
        v
    globalBone * boneOffset
        |
        v
    CPU skinning
        |
        v
    VBO GL_DYNAMIC_DRAW
        |
        v
    modelo realmente animado

Tambem suporta FBX com animacao rigida de nodes sem bones.

Ao carregar um FBX com animacao:
- o primeiro clip comeca a tocar automaticamente;
- Animation > Play/Pause controla playback;
- Space tambem Play/Pause;
- Previous/Next troca clips;
- Edit Box Anim troca indice;
- Edit Box Speed muda velocidade;
- Edit Box Time busca um tempo do clip.

O modelo inteiro nao gira mais automaticamente enquanto a animacao toca.


FIX: FBX ANIMATION TORCIDA
==========================

A versao anterior calculava:

    globalBone * boneOffset

Isto e incompleto para Assimp/FBX.

Agora usa:

    inverse(sceneRoot) * globalBone * boneOffset

O inverse(sceneRoot) remove a conversao de eixo/unidade que o FBX
frequentemente coloca no root node.

A pose estatica tambem passou a usar:

    inverse(sceneRoot) * globalMeshNode

portanto pose estatica e pose animada estao no mesmo espaco.

DEBUG
=====
Ao carregar FBX agora aparece:

    FBX bone mapping: total=N missing=0

O esperado e:

    missing=0

Se missing for maior que zero, envie as linhas:

    FBX WARNING: bone node not found: ...

porque esse FBX usa nomes/hierarquia de armature que precisam de outro
mapeamento.


MIXAMO FBX - CORRECAO DE ANIMACAO
=================================

Esta versao troca o avaliador de animacao para usar aiMatrix4x4 do
proprio Assimp do inicio ao fim.

Isso e importante para FBX do Mixamo porque o importador pode gerar
nodes auxiliares para:
- PreRotation
- PostRotation
- RotationPivot
- ScalingPivot

O player preserva a arvore inteira do Assimp e calcula:

    local animated TRS
          |
          v
    parentGlobal * local
          |
          v
    inverse(sceneRoot)
          *
    boneGlobal
          *
    boneOffset
          |
          v
    CPU skinning
          |
          v
    VBO OpenGL

Tambem foi adicionado fallback de nome para nodes com sufixo:

    _$AssimpFbx$_...

DEBUG esperado:

    FBX bone mapping: total=N missing=0
    Mixamo/FBX debug:
      clip=Running
      channels=...
      helper_channels=...
      duration=...
      tps=...

Se o FBX foi baixado no Mixamo, prefira para testar:
- Format: FBX Binary
- Skin: With Skin
- Frames per Second: 30
- Keyframe Reduction: None

Para uma corrida no lugar, marque "In Place" no Mixamo.


FIX DE COMPILACAO ASSIMP 5.3.1 / i686
=====================================

A build i686 do Assimp usada pelo projeto nao expoe os overloads:

    aiMatrix4x4 * aiVector3D
    aiMatrix3x3 * aiVector3D
    aiVector3D * float

O renderer agora usa funcoes manuais:

    ai_transform_point_manual()
    ai_transform_vector_manual()
    ai_vec3_scale_manual()
    ai_vec3_add_scaled_manual()

Isso elimina os erros "no match for operator*".


FIX DEFINITIVO DE LINK: SEM MATEMATICA ASSIMP NO RENDERER
=========================================================

O renderer nao usa mais:

    aiMatrix4x4
    aiMatrix3x3
    aiVector3D

nem seus:
    constructors
    operator*
    Inverse()
    Transpose()
    Normalize()

Portanto nao existem mais referencias de linker aos templates de matematica
do Assimp.

O Assimp continua sendo usado SOMENTE pelo loader FBX.

O player usa:
    SkinMat4
    SkinMat3
    SkinVec3
    SkinQuat

com multiplicacao, inversa-transposta, SLERP e transformacao implementadas
diretamente no renderer.cpp.


ORBIT CAMERA - BOTAO DIREITO
============================

Controles:

    Segurar botao direito + arrastar horizontal:
        gira Yaw

    Segurar botao direito + arrastar vertical:
        gira Pitch

    Duplo clique com botao direito:
        reseta a camera

Implementacao Win32:

    WM_RBUTTONDOWN
        SetCapture()

    WM_MOUSEMOVE + MK_RBUTTON
        calcula dx/dy
        renderer_orbit_drag()

    WM_RBUTTONUP
        ReleaseCapture()

A camera possui:

    renderer.orbit_yaw
    renderer.orbit_pitch
    renderer.orbit_distance

O pitch e limitado em aproximadamente +/-89 graus para impedir flip.
A animacao FBX continua tocando enquanto a camera e rotacionada.


BOX COLLIDER + GRAVITY + GDI/WIN32 PANEL
========================================

Novo painel nativo na janela:

    Box Collider / Physics

    Size X/Y/Z:
        tamanho do collider

    Offset X/Y/Z:
        deslocamento do collider em relacao ao corpo/modelo

    [ ] Gravity:
        ativa/desativa gravidade

    [x] Show Collider:
        mostra o box collider em wireframe verde

    [Reset Physics]:
        zera altura e velocidade vertical

    [Apply]:
        aplica Size/Offset e estado dos checkboxes

FISICA
======
Primeira versao:
- AABB/box collider;
- gravidade vertical = -9.81;
- integracao semi-explicita simples;
- dt limitado a 0.05 s para evitar salto apos travamento/debug;
- plano de chao em Y = -1.75;
- sem bounce;
- sem rotacao fisica;
- sem colisao box-vs-box ainda.

A colisao com o chao usa:

    bottom =
        bodyY
        + offsetY
        - sizeY/2

Se:

    bottom <= groundY

o corpo e corrigido para fora do chao e:

    verticalVelocity = 0

O modelo animado continua animando normalmente; a gravidade move o corpo
inteiro por fora da animacao.

VISUALIZACAO
============
O collider e desenhado com o cube mesh existente em GL_LINE.
Como o cube base vai de -1 a +1:

    colliderScale =
        size * 0.5

Assim Size X/Y/Z corresponde ao tamanho total do box.


MIXAMO PIVOT FIX REINTEGRADO
============================

Esta versao mantem conjuntamente:

- GLB loader
- FBX loader
- leitura de animation/bones
- Mixamo animation player
- orbit camera com botao direito
- Box Collider
- Gravity ON/OFF
- GDI/Win32 Edit Boxes
- visualizacao do collider

No arquivo:

    src\fbx_loader.cpp

foi reintegrado:

    #include <assimp/config.h>

e imediatamente apos:

    Assimp::Importer importer;

fica:

    importer.SetPropertyBool(
        AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS,
        false
    );

Esta propriedade e aplicada ANTES de:

    importer.ReadFile(...)

Console esperado:

    FBX importer: PRESERVE_PIVOTS=FALSE (Mixamo mode)

Isso evita que o projeto de Box Collider volte a perder a correcao
especifica para animacoes Mixamo.


FIX: FRAME GDI DO BOX COLLIDER NAO APARECIA
===========================================

Problema:
O painel antigo era um BS_GROUPBOX criado diretamente como filho da mesma
janela cujo DC possui o contexto OpenGL. SwapBuffers podia apagar/ocultar
o frame visual no Windows 7.

Correcao:
- classe GDI separada: GL33PhysicsPanel
- DC/WM_PAINT proprio
- frame desenhado explicitamente via GDI
- titulo "Box Collider / Physics" desenhado no painel
- Edit Boxes/checkboxes/botao agora sao filhos do painel
- WM_COMMAND do painel e encaminhado para a janela principal
- janela OpenGL agora usa:

    WS_CLIPCHILDREN
    WS_CLIPSIBLINGS

Assim o OpenGL nao desenha sobre o painel GDI.

O frame agora e desenhado no:

    physics_panel_proc()
        WM_PAINT
            Rectangle(...)
            TextOutA(...)

e nao depende do BS_GROUPBOX do Windows.


PROJECTILES + PLANO VERDE 10x10
===============================

Este projeto continua a partir da versao que ja tinha:
- Mixamo pivot fix;
- FBX/GLB;
- animacao;
- orbit camera RMB;
- GDI Physics Panel;
- Box Collider;
- Gravity.

PLANO
=====
Foi adicionado um mesh procedural de plano:

    tamanho: 10 x 10
    centro : X=0 Z=0
    altura : collider.ground_y

Default:

    ground_y = -1.75

O plano e verde e usa o mesmo Y da colisao fisica.

TIRO
====
Cada bolinha nasce EXATAMENTE no centro do Box Collider:

    X = collider.offset_x
    Y = collider.body_y + collider.offset_y
    Z = collider.offset_z

A direcao e o forward da Orbit Camera.

Controles:

    F
        dispara

    Shoot [F]
        dispara pelo painel GDI

    Bullet Speed
        velocidade inicial
        default = 12.0

    Radius
        raio da esfera
        default = 0.12

    Clear Balls
        remove todos os projeteis

FISICA DAS BOLINHAS
===================
- gravidade = -9.81;
- lifetime = 8 segundos;
- colisao com o plano somente dentro de X/Z = -5..+5;
- bounce = 0.45;
- atrito horizontal = 0.82;
- maximo de 128 projeteis ativos.

RENDER
======
Foi criado um UV sphere leve:

    stacks = 10
    slices = 16

adequado para a maquina i686/Intel HD.

A esfera e reutilizada para todos os tiros; cada projetil so muda:
- position;
- radius;
- velocity;
- lifetime.

Isto evita criar VAO/VBO novo a cada disparo.


SHENMUE-STYLE CHARACTER CONTROLS
================================

Esta versao continua EXATAMENTE a partir do projeto com:
- Mixamo pivot fix;
- GDI Physics Panel;
- Box Collider;
- Gravity;
- orbit camera RMB;
- plano verde 10x10;
- tiros de bolinha.

CONTROLES DO PERSONAGEM
=======================

    W / SETA CIMA
        anda PARA FRENTE na direcao em que o personagem esta olhando

    S / SETA BAIXO
        anda PARA TRAS

    A / SETA ESQUERDA
        gira personagem para a ESQUERDA

    D / SETA DIREITA
        gira personagem para a DIREITA

Este e um controle tipo Shenmue / tank controls:

    esquerda/direita NAO fazem strafe.
    Eles mudam o heading do personagem.

Default:

    forward speed  = 2.5
    backward speed = 1.6
    turn speed     = 120 graus/segundo

CAMERA
======

O botao direito continua controlando APENAS a orbit camera.

A camera nao e mais necessaria para atirar.

TIRO
====

    F

dispara uma bolinha NA DIRECAO DO PERSONAGEM.

Nao precisa:
- clicar botao direito;
- estar fazendo orbit;
- apontar a camera.

O tiro usa:

    body_yaw

Direcao:

    X = -sin(yaw)
    Z = -cos(yaw)

O spawn continua vindo do Box Collider, com pequeno deslocamento para frente
para a esfera nao nascer dentro do corpo.

DIRECTION CUBE
==============

Foi adicionado um cubinho azul comprido na frente do Box Collider.

Ele mostra visualmente:

    "para onde o personagem esta apontando"

Esse marcador gira junto com body_yaw.

TRANSFORM DO PERSONAGEM
=======================

Agora o corpo possui:

    body_x
    body_y
    body_z
    body_yaw

A animacao Mixamo fica em espaco local.
Depois a engine aplica:

    character rotation
    character translation

Assim a animacao de corrida e o movimento no mundo sao independentes.


100x100 MAP + FRONT FIX + WAYPOINT AI
=====================================

CONTINUA DA VERSAO ANTERIOR
===========================
Mantido:
- Mixamo pivot fix;
- FBX/GLB;
- skeletal animation;
- orbit camera RMB;
- GDI Physics Panel;
- Box Collider;
- Gravity;
- bolinhas/projeteis;
- Shenmue tank controls.

CORRECAO DA FRENTE
==================
O modelo carregado estava visualmente olhando para o lado contrario do tiro.

Agora a engine define:

    yaw = 0 -> +Z

Forward:

    X = sin(yaw)
    Z = cos(yaw)

Isso foi aplicado de forma consistente em:
- movimento W/S;
- tiro F;
- direction cube azul;
- AI waypoint.

MAPA
====
Plano verde agora:

    100 x 100

Limites:

    X = -50 .. +50
    Z = -50 .. +50

Jogador e AI sao limitados aproximadamente a:

    -49 .. +49

Projeteis colidem com o plano dentro de:

    -50 .. +50

WAYPOINT AI
===========
Foi adicionado um segundo personagem IA usando O MESMO modelo FBX/GLB carregado.

Nao duplica VBO/mesh:
o renderer simplesmente desenha renderer.model_parts novamente com outra
transformacao world.

Rota default:

    (-15,-15)
    (  0,-22)
    ( 15,-15)
    ( 22,  0)
    ( 15, 15)
    (  0, 22)
    (-15, 15)
    (-22,  0)
    e repete

Algoritmo:

    target = waypoints[current]

    dx = target.x - ai.x
    dz = target.z - ai.z

    desiredYaw = atan2(dx, dz)

    yawError = wrapPi(desiredYaw - ai.yaw)

    ai.yaw += clamp(
        yawError,
        -turnSpeed*dt,
        +turnSpeed*dt
    )

    ai.x += sin(ai.yaw) * speed * dt
    ai.z += cos(ai.yaw) * speed * dt

Quando chega proximo:

    distance <= reach_radius

avanca para o proximo waypoint.

GDI AI
======
Novos controles:

    [x] AI Waypoint
        liga/desliga o NPC

    [x] Show Path
        mostra os waypoints

    AI Speed: [2.0]

    [Reset AI]

O waypoint alvo atual aparece laranja.
Os outros aparecem amarelos.

TIRO F
======
F agora e lido usando:

    GetAsyncKeyState('F')

Isso significa que NAO depende:
- de clicar RMB;
- da orbit camera;
- do foco estar no HWND principal em vez de um Edit Box filho.

O tiro segue a frente do personagem, nao a camera.


LIGHTING + CHASE CAMERA + AMMO/WEAPON PICKUPS
==============================================

Base preservada:
- Mixamo pivot fix;
- map 100x100;
- Shenmue controls;
- F shoot;
- Box Collider/Gravity;
- GDI panel;
- waypoint AI using loaded model.

ILUMINACAO
==========
O shader agora tem:
- directional key light;
- secondary fill light;
- sky/hemi light;
- ambient mais forte.

A luz e direcional e ilumina toda a area 100x100.

CHASE CAMERA
============
A camera agora segue:

    collider.body_x
    collider.body_y
    collider.body_z

e gira junto com:

    collider.body_yaw

Base:

    chaseYaw =
        bodyYaw
        + PI
        + orbitYaw

Assim ela fica atras do personagem.
RMB continua alterando orbitYaw/orbitPitch.

Defaults:

    distance = 6.0
    target height = 0.85
    pitch = 0.10

AMMO
====
Comeca:

    10 / 10

Cada tiro F:

    -1

Quando ammo chega em zero:

    renderer_fire_projectile()

bloqueia o disparo e mostra:

    NO AMMO - PICK UP ORANGE ITEM (+5)

PICKUPS
=======
Existem 6 pickups de arma/municao no mapa.

Visual:
- caixa laranja girando;
- barra amarela acima.

Ao tocar no item com ammo < 10:

    ammo = min(10, ammo + 5)

Cada pickup e consumido depois de coletado.

Posicoes:
    (  7,   6)
    (-10,   9)
    ( 18, -12)
    (-23, -17)
    (  2,  28)
    ( 31,  22)

Raio de coleta:

    1.15

GDI
===
O painel mostra:

    Ammo: N / 10

    Orange weapon item = +5 shots (max 10)

A label so recebe SetWindowText quando o numero realmente muda.


CAMERA ROTATION FIX + AI ATTACK WITH SHOOTING
=============================================

CAMERA FIX
==========
Antes:

    cameraYaw =
        bodyYaw
        + PI
        + orbitYaw

Isso fazia A/D girar:
- personagem;
- camera;

ao mesmo tempo.

Agora:

    cameraYaw =
        PI
        + orbitYaw

A camera continua seguindo:

    body_x
    body_y
    body_z

mas NAO herda mais:

    body_yaw

Resultado:

    A / D
        gira somente o personagem

    RMB + mouse
        gira somente a camera

    W / S
        personagem se move e a camera acompanha a POSICAO.

AI ATTACK
=========
A IA agora possui:

    attack_range = 20.0
    fire_interval = 1.0 segundo
    projectile_speed = 18.0
    turn_speed = 3.5 rad/s

Consideramos:

    1 unidade da engine ~= 1 metro

Quando:

    distance(player, AI) <= 20

a IA entra em ATTACK MODE:

    1. para de andar nos waypoints;
    2. gira em direcao ao jogador;
    3. espera ficar alinhada;
    4. dispara a cada ~1 segundo.

Quando o jogador sai de 20m:

    volta para WAYPOINT PATROL.

AI PROJECTILES
==============
Tiro da IA:
- esfera vermelha;
- hostile = true;
- gravity_scale = 0;
- tiro reto;
- velocidade = 18.

Tiro do jogador:
- continua amarelo/laranja;
- hostile = false;
- gravity_scale = 1;
- continua com a fisica de bolinha existente.

PLAYER HIT / HEALTH
===================
Projeteis hostis testam colisao contra o Box Collider do jogador.

Cada acerto:

    health -= 10

Default:

    Health = 100 / 100

O painel GDI mostra:

    Health: N / 100
    AI Attack: <= 20m | red bullets = hostile

Reset Physics tambem restaura:

    Health = 100
