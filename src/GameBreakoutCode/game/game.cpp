/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#include <algorithm>
#include <sstream>
#include <iostream>

#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include "game_object.h"
#include "particle_generator.h"
#include "post_processor.h"
#include "power_up.h"
#include "ball_object.h"
#include "LearnOpenGL/filesystem.h"
#include "text_renderer.h"

#include <irrklang/irrKlang.h>
using namespace irrklang;

// Game-related State data
SpriteRenderer *Renderer;
GameObject *Player;
BallObject *Ball;
ParticleGenerator *Particles;
PostProcessor *Effects;
GLfloat ShakeTime = 0.0f;
ISoundEngine *SoundEngine = createIrrKlangDevice();
TextRenderer *Text;

GLboolean CheckCollision(GameObject &one, GameObject &two);
Collision CheckCollision(BallObject &one, GameObject &two);
Direction VectorDirection(glm::vec2 target);

// powerups
bool ShouldSpawn(unsigned int chance);
void ActivatePowerUp(PowerUp &powerUp);
bool IsOtherPowerUpActive(std::vector<PowerUp> &powerUps, std::string type);

Game::Game(unsigned int width, unsigned int height)
    : State(GAME_ACTIVE), Keys(), Width(width), Height(height), Lives(3)
{
}

Game::~Game()
{
    delete Renderer;
    delete Player;
    delete Ball;
    delete Particles;
    delete Effects;
    delete SoundEngine;
    delete Text;
}

void Game::Init()
{
    // play sound looper
    SoundEngine->play2D(FileSystem::getPath("resources/audio/breakout.mp3").c_str(), GL_TRUE);

    // init Text Renderer
    Text = new TextRenderer(this->Width, this->Height);
    Text->Load(FileSystem::getPath("resources/fonts/sanjichuyaoxingkai.ttf").c_str(), 24);

    // FileSystem::getPath("resources/textures/awesomeface.png").c_str()
    // load shaders
    ResourceManager::LoadShader("src/GameBreakoutCode/shaders/sprite.vs", "src/GameBreakoutCode/shaders/sprite.frag", nullptr, "sprite");
    ResourceManager::LoadShader("src/GameBreakoutCode/shaders/particle.vs", "src/GameBreakoutCode/shaders/particle.frag", nullptr, "particle");
    ResourceManager::LoadShader("src/GameBreakoutCode/shaders/post_processing.vs", "src/GameBreakoutCode/shaders/post_processing.frag", nullptr, "postprocessing");
    // configure shaders
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
                                      static_cast<float>(this->Height), 0.0f, -1.0f, 1.0f);
    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    ResourceManager::GetShader("particle").Use().SetInteger("sprite", 0);
    ResourceManager::GetShader("particle").SetMatrix4("projection", projection);

    // load textures
    ResourceManager::LoadTexture("resources/textures/background.jpg", false, "background");
    ResourceManager::LoadTexture("resources/textures/awesomeface.png", true, "face");
    ResourceManager::LoadTexture("resources/textures/block.png", false, "block");
    ResourceManager::LoadTexture("resources/textures/block_solid.png", false, "block_solid");
    ResourceManager::LoadTexture("resources/textures/paddle.png", true, "paddle");
    // ResourceManager::LoadTexture("resources/textures/particle.png", true, "particle");
    ResourceManager::LoadTexture("resources/textures/test01.png", true, "particle");
    ResourceManager::LoadTexture("resources/textures/powerup_speed.png", true, "powerup_speed");
    ResourceManager::LoadTexture("resources/textures/powerup_sticky.png", true, "powerup_sticky");
    ResourceManager::LoadTexture("resources/textures/powerup_increase.png", true, "powerup_increase");
    ResourceManager::LoadTexture("resources/textures/powerup_confuse.png", true, "powerup_confuse");
    ResourceManager::LoadTexture("resources/textures/powerup_chaos.png", true, "powerup_chaos");
    ResourceManager::LoadTexture("resources/textures/powerup_passthrough.png", true, "powerup_passthrough");

    // set render-specific controls
    Renderer = new SpriteRenderer(ResourceManager::GetShader("sprite"));
    // init Particles
    Particles = new ParticleGenerator(ResourceManager::GetShader("particle"), ResourceManager::GetTexture("particle"), 500);
    Effects = new PostProcessor(ResourceManager::GetShader("postprocessing"), this->Width, this->Height);
    // load levels
    GameLevel one;
    one.Load("src/GameBreakoutCode/levels/one.lvl", this->Width, this->Height / 2);
    GameLevel two;
    two.Load("src/GameBreakoutCode/levels/two.lvl", this->Width, this->Height / 2);
    GameLevel three;
    three.Load("src/GameBreakoutCode/levels/three.lvl", this->Width, this->Height / 2);
    GameLevel four;
    four.Load("src/GameBreakoutCode/levels/four.lvl", this->Width, this->Height / 2);
    GameLevel five;
    five.Load("src/GameBreakoutCode/levels/five.lvl", this->Width, this->Height / 2);
    GameLevel six;
    six.Load("src/GameBreakoutCode/levels/six.lvl", this->Width, this->Height / 2);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Levels.push_back(three);
    this->Levels.push_back(four);
    this->Levels.push_back(five);
    this->Levels.push_back(six);
    this->Level = 0;
    // configure game objects
    glm::vec2 playerPos = glm::vec2(this->Width / 2.0f - PLAYER_SIZE.x / 2.0f, this->Height - PLAYER_SIZE.y);
    Player = new GameObject(playerPos, PLAYER_SIZE, ResourceManager::GetTexture("paddle"));

    // init ball
    glm::vec2 ballPos = playerPos + glm::vec2(PLAYER_SIZE.x / 2 - BALL_RADIUS, -BALL_RADIUS * 2);
    Ball = new BallObject(ballPos, BALL_RADIUS, INITIAL_BALL_VELOCITY,
                          ResourceManager::GetTexture("face"));
}

void Game::Update(float dt)
{
    // Ball position update
    Ball->Move(dt, this->Width);
    // Ball Collision check
    this->DoCollisions();

    // update particles
    Particles->Update(dt, *Ball, 2, glm::vec2(Ball->Radius / 2.0f));

    // update PowerUps
    this->UpdatePowerUps(dt);

    // Effects Shake
    if (ShakeTime > 0.0f)
    {
        ShakeTime -= dt;
        if (ShakeTime <= 0.0f)
            Effects->Shake = false;
    }

    // check loss condition
    if (Ball->Position.y >= this->Height) // did ball reach bottom edge?
    {
        --this->Lives;
        // 玩家是否已失去所有生命值? : 游戏结束
        if (this->Lives == 0)
        {
            this->ResetLevel();
            this->State = GAME_MENU;
        }
        this->ResetPlayer();
    }

    if (this->State == GAME_ACTIVE && this->Levels[this->Level].IsCompleted())
    {
        this->ResetLevel();
        this->ResetPlayer();
        Effects->Chaos = GL_TRUE;
        this->State = GAME_WIN;
    }
}

void Game::ProcessInput(float dt)
{
    if (this->State == GAME_MENU)
    {
        if (this->Keys[GLFW_KEY_ENTER] && !this->KeysProcessed[GLFW_KEY_ENTER])
        {
            this->State = GAME_ACTIVE;
            this->KeysProcessed[GLFW_KEY_ENTER] = true;
        }
        if (this->Keys[GLFW_KEY_W] && !this->KeysProcessed[GLFW_KEY_W])
        {
            this->Level = (this->Level + 1) % 4;
            this->KeysProcessed[GLFW_KEY_W] = true;
        }
        if (this->Keys[GLFW_KEY_S] && !this->KeysProcessed[GLFW_KEY_S])
        {
            if (this->Level > 0)
                --this->Level;
            else
                this->Level = 3;
            // this->Level = (this->Level - 1) % 4;
            this->KeysProcessed[GLFW_KEY_S] = true;
        }
    }
    if (this->State == GAME_WIN)
    {
        if (this->Keys[GLFW_KEY_ENTER])
        {
            this->KeysProcessed[GLFW_KEY_ENTER] = true;
            Effects->Chaos = false;
            this->State = GAME_MENU;
        }
    }
    if (this->State == GAME_ACTIVE)
    {
        GLfloat velocity = PLAYER_VELOCITY * dt;
        // 移动玩家挡板
        if (this->Keys[GLFW_KEY_A])
        {
            if (Player->Position.x >= 0)
            {
                Player->Position.x -= velocity;
                if (Ball->Stuck)
                    Ball->Position.x -= velocity;
            }
        }
        if (this->Keys[GLFW_KEY_D])
        {
            if (Player->Position.x <= this->Width - Player->Size.x)
            {
                Player->Position.x += velocity;
                if (Ball->Stuck)
                    Ball->Position.x += velocity;
            }
        }
        if (this->Keys[GLFW_KEY_SPACE])
            Ball->Stuck = false;
    }
}

void Game::Render()
{
    if (this->State == GAME_ACTIVE || this->State == GAME_MENU)
    {
        Effects->BeginRender();
        // draw background
        Renderer->DrawSprite(ResourceManager::GetTexture("background"), glm::vec2(0.0f, 0.0f), glm::vec2(this->Width, this->Height), 0.0f);
        // draw level
        this->Levels[this->Level].Draw(*Renderer);
        // draw player
        Player->Draw(*Renderer);
        // draw PowerUps
        for (PowerUp &powerUp : this->PowerUps)
        {
            if (!powerUp.Destroyed)
            {
                powerUp.Draw(*Renderer);
            }
        }

        // draw particles
        Particles->Draw();
        // draw ball
        Ball->Draw(*Renderer);

        std::stringstream ss;
        ss << this->Lives;
        Text->RenderText("Lives:" + ss.str(), 5.0f, 5.0f, 1.0f);

        Effects->EndRender();
        // render postprocessing quad
        Effects->Render(glfwGetTime());
    }

    if (this->State == GAME_MENU)
    {
        Text->RenderText("Press ENTER to start", 250.0f, Height / 2, 1.0f);
        Text->RenderText("Press W or S to select level", 245.0f, Height / 2 + 20.0f, 0.75f);
    }

    if (this->State == GAME_WIN)
    {
        Text->RenderText(
            "You WIN!!!", 320.0, Height / 2 - 20.0, 1.0, glm::vec3(0.0, 1.0, 0.0));
        Text->RenderText(
            "Press ENTER to retry or ESC to quit", 130.0, Height / 2, 1.0, glm::vec3(1.0, 1.0, 0.0));
    }
}

void Game::DoCollisions()
{
    for (GameObject &box : this->Levels[this->Level].Bricks)
    {
        if (!box.Destroyed)
        {
            Collision collision = CheckCollision(*Ball, box);
            if (std::get<0>(collision)) // if collision is true
            {
                // destroy block if not solid
                if (!box.IsSolid)
                {
                    box.Destroyed = true;
                    this->SpawnPowerUps(box);
                    SoundEngine->play2D(FileSystem::getPath("resources/audio/bleep.mp3").c_str(), false);
                }
                else
                { // if block is solid, enable shake effect
                    ShakeTime = 0.05f;
                    Effects->Shake = true;
                    SoundEngine->play2D(FileSystem::getPath("resources/audio/bleep.mp3").c_str(), false);
                }
                // collision resolution
                Direction dir = std::get<1>(collision);
                glm::vec2 diff_vector = std::get<2>(collision);
                if (!(Ball->PassThrough && !box.IsSolid)) // don't do collision resolution on non-solid bricks if pass-through is activated
                {
                    if (dir == LEFT || dir == RIGHT) // horizontal collision
                    {
                        Ball->Velocity.x = -Ball->Velocity.x; // reverse horizontal velocity
                        // relocate
                        float penetration = Ball->Radius - std::abs(diff_vector.x);
                        if (dir == LEFT)
                            Ball->Position.x += penetration; // move ball to right
                        else
                            Ball->Position.x -= penetration; // move ball to left;
                    }
                    else // vertical collision
                    {
                        Ball->Velocity.y = -Ball->Velocity.y; // reverse vertical velocity
                        // relocate
                        float penetration = Ball->Radius - std::abs(diff_vector.y);
                        if (dir == UP)
                            Ball->Position.y -= penetration; // move ball bback up
                        else
                            Ball->Position.y += penetration; // move ball back down
                    }
                }
            }
        }
    }

    // also check collisions on PowerUps and if so, activate them
    for (PowerUp &powerUp : this->PowerUps)
    {
        if (!powerUp.Destroyed)
        {
            // first check if powerup passed bottom edge, if so: keep as inactive and destroy
            if (powerUp.Position.y >= this->Height)
                powerUp.Destroyed = true;

            if (CheckCollision(*Player, powerUp))
            { // collided with player, now activate powerup
                ActivatePowerUp(powerUp);
                powerUp.Destroyed = true;
                powerUp.Activated = true;
                SoundEngine->play2D(FileSystem::getPath("resources/audio/powerup.wav").c_str(), false);
            }
        }
    }

    // and finally check collisions for player pad (unless stuck)
    Collision result = CheckCollision(*Ball, *Player);
    if (!Ball->Stuck && std::get<0>(result))
    {
        // check where it hit the board, and change velocity based on where it hit the board
        float centerBoard = Player->Position.x + Player->Size.x / 2.0f;
        float distance = (Ball->Position.x + Ball->Radius) - centerBoard;
        float percentage = distance / (Player->Size.x / 2.0f);
        // then move accordingly
        float strength = 2.0f;
        glm::vec2 oldVelocity = Ball->Velocity;
        Ball->Velocity.x = INITIAL_BALL_VELOCITY.x * percentage * strength;
        // Ball->Velocity.y = -Ball->Velocity.y;
        Ball->Velocity = glm::normalize(Ball->Velocity) * glm::length(oldVelocity); // keep speed consistent over both axes (multiply by length of old velocity, so total strength is not changed)
        // fix sticky paddle
        Ball->Velocity.y = -1.0f * abs(Ball->Velocity.y);

        // if Sticky powerup is activated, also stick ball to paddle once new velocity vectors were calculated
        Ball->Stuck = Ball->Sticky;

        SoundEngine->play2D(FileSystem::getPath("resources/audio/powerup.wav").c_str(), false);
    }
}

void Game::UpdatePowerUps(float dt)
{
    for (PowerUp &powerUp : this->PowerUps)
    {
        powerUp.Position += powerUp.Velocity * dt;
        if (powerUp.Activated)
        {
            powerUp.Duration -= dt;

            if (powerUp.Duration <= 0.0f)
            {
                // remove powerup from list (will later be removed)
                powerUp.Activated = false;
                // deactivate effects
                if (powerUp.Type == "sticky")
                {
                    if (!IsOtherPowerUpActive(this->PowerUps, "sticky"))
                    { // only reset if no other PowerUp of type sticky is active
                        Ball->Sticky = false;
                        Player->Color = glm::vec3(1.0f);
                    }
                }
                else if (powerUp.Type == "pass-through")
                {
                    if (!IsOtherPowerUpActive(this->PowerUps, "pass-through"))
                    { // only reset if no other PowerUp of type pass-through is active
                        Ball->PassThrough = false;
                        Ball->Color = glm::vec3(1.0f);
                    }
                }
                else if (powerUp.Type == "confuse")
                {
                    if (!IsOtherPowerUpActive(this->PowerUps, "confuse"))
                    { // only reset if no other PowerUp of type confuse is active
                        Effects->Confuse = false;
                    }
                }
                else if (powerUp.Type == "chaos")
                {
                    if (!IsOtherPowerUpActive(this->PowerUps, "chaos"))
                    { // only reset if no other PowerUp of type chaos is active
                        Effects->Chaos = false;
                    }
                }
            }
        }
    }
    // Remove all PowerUps from vector that are destroyed AND !activated (thus either off the map or finished)
    // Note we use a lambda expression to remove each PowerUp which is destroyed and not activated
    this->PowerUps.erase(std::remove_if(this->PowerUps.begin(), this->PowerUps.end(),
                                        [](const PowerUp &powerUp)
                                        { return powerUp.Destroyed && !powerUp.Activated; }),
                         this->PowerUps.end());
}

bool ShouldSpawn(unsigned int chance)
{
    unsigned int random = rand() % chance;
    return random == 0;
}

void Game::SpawnPowerUps(GameObject &block)
{
    if (ShouldSpawn(75)) // 1 in 75 chance
        this->PowerUps.push_back(PowerUp("speed", glm::vec3(0.5f, 0.5f, 1.0f), 0.0f, block.Position, ResourceManager::GetTexture("powerup_speed")));
    if (ShouldSpawn(75))
        this->PowerUps.push_back(PowerUp("sticky", glm::vec3(1.0f, 0.5f, 1.0f), 20.0f, block.Position, ResourceManager::GetTexture("powerup_sticky")));
    if (ShouldSpawn(75))
        this->PowerUps.push_back(PowerUp("pass-through", glm::vec3(0.5f, 1.0f, 0.5f), 10.0f, block.Position, ResourceManager::GetTexture("powerup_passthrough")));
    if (ShouldSpawn(75))
        this->PowerUps.push_back(PowerUp("pad-size-increase", glm::vec3(1.0f, 0.6f, 0.4), 0.0f, block.Position, ResourceManager::GetTexture("powerup_increase")));
    if (ShouldSpawn(15)) // Negative powerups should spawn more often
        this->PowerUps.push_back(PowerUp("confuse", glm::vec3(1.0f, 0.3f, 0.3f), 15.0f, block.Position, ResourceManager::GetTexture("powerup_confuse")));
    if (ShouldSpawn(15))
        this->PowerUps.push_back(PowerUp("chaos", glm::vec3(0.9f, 0.25f, 0.25f), 15.0f, block.Position, ResourceManager::GetTexture("powerup_chaos")));
}

void ActivatePowerUp(PowerUp &powerUp)
{
    if (powerUp.Type == "speed")
    {
        Ball->Velocity *= 1.2;
    }
    else if (powerUp.Type == "sticky")
    {
        Ball->Sticky = true;
        Player->Color = glm::vec3(1.0f, 0.5f, 1.0f);
    }
    else if (powerUp.Type == "pass-through")
    {
        Ball->PassThrough = true;
        Ball->Color = glm::vec3(1.0f, 0.5f, 0.5f);
    }
    else if (powerUp.Type == "pad-size-increase")
    {
        Player->Size.x += 50;
    }
    else if (powerUp.Type == "confuse")
    {
        if (!Effects->Chaos)
            Effects->Confuse = true; // only activate if chaos wasn't already active
    }
    else if (powerUp.Type == "chaos")
    {
        if (!Effects->Confuse)
            Effects->Chaos = true;
    }
}

bool IsOtherPowerUpActive(std::vector<PowerUp> &powerUps, std::string type)
{
    // Check if another PowerUp of the same type is still active
    // in which case we don't disable its effect (yet)
    for (const PowerUp &powerUp : powerUps)
    {
        if (powerUp.Activated)
            if (powerUp.Type == type)
                return true;
    }
    return false;
}

GLboolean CheckCollision(GameObject &one, GameObject &two) // AABB - AABB collision
{
    // x轴方向碰撞？
    bool collisionX = one.Position.x + one.Size.x >= two.Position.x &&
                      two.Position.x + two.Size.x >= one.Position.x;
    // y轴方向碰撞？
    bool collisionY = one.Position.y + one.Size.y >= two.Position.y &&
                      two.Position.y + two.Size.y >= one.Position.y;
    // 只有两个轴向都有碰撞时才碰撞
    return collisionX && collisionY;
}
Collision CheckCollision(BallObject &one, GameObject &two) // AABB - Circle collision
{
    // 获取圆的中心
    glm::vec2 center(one.Position + one.Radius);
    // 计算AABB的信息（中心、半边长）
    glm::vec2 aabb_half_extents(two.Size.x / 2, two.Size.y / 2);
    glm::vec2 aabb_center(
        two.Position.x + aabb_half_extents.x,
        two.Position.y + aabb_half_extents.y);
    // 获取两个中心的差矢量
    glm::vec2 difference = center - aabb_center;
    glm::vec2 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);
    // AABB_center加上clamped这样就得到了碰撞箱上距离圆最近的点closest
    glm::vec2 closest = aabb_center + clamped;
    // 获得圆心center和最近点closest的矢量并判断是否 length <= radius
    difference = closest - center;
    if (glm::length(difference) <= one.Radius)
        return std::make_tuple(GL_TRUE, VectorDirection(difference), difference);
    else
        return std::make_tuple(GL_FALSE, UP, glm::vec2(0, 0));
}
Direction VectorDirection(glm::vec2 target)
{
    glm::vec2 compass[] = {
        glm::vec2(0.0f, 1.0f),  // 上
        glm::vec2(1.0f, 0.0f),  // 右
        glm::vec2(0.0f, -1.0f), // 下
        glm::vec2(-1.0f, 0.0f)  // 左
    };
    GLfloat max = 0.0f;
    GLuint best_match = -1;
    for (GLuint i = 0; i < 4; i++)
    {
        GLfloat dot_product = glm::dot(glm::normalize(target), compass[i]);
        if (dot_product > max)
        {
            max = dot_product;
            best_match = i;
        }
    }
    return (Direction)best_match;
}

void Game::ResetLevel()
{
    if (this->Level == 0)
    {
        this->Levels[0].Load("src/GameBreakoutCode/levels/one.lvl", this->Width, this->Height * 0.5f);
    }
    else if (this->Level == 1)
    {
        this->Levels[1].Load("src/GameBreakoutCode/levels/two.lvl", this->Width, this->Height * 0.5f);
    }
    else if (this->Level == 2)
    {
        this->Levels[2].Load("src/GameBreakoutCode/levels/three.lvl", this->Width, this->Height * 0.5f);
    }
    else if (this->Level == 3)
    {
        this->Levels[3].Load("src/GameBreakoutCode/levels/four.lvl", this->Width, this->Height * 0.5f);
    }

    this->Lives = 3;
}

void Game::ResetPlayer()
{
    // Reset player/ball stats
    Player->Size = PLAYER_SIZE;
    Player->Position = glm::vec2(this->Width / 2 - PLAYER_SIZE.x / 2, this->Height - PLAYER_SIZE.y);
    Ball->Reset(Player->Position + glm::vec2(PLAYER_SIZE.x / 2 - BALL_RADIUS, -(BALL_RADIUS * 2)), INITIAL_BALL_VELOCITY);
}

// test code
void Game::BallReset()
{
    Ball->Reset(Player->Position + glm::vec2(PLAYER_SIZE.x / 2 - BALL_RADIUS, -BALL_RADIUS * 2), INITIAL_BALL_VELOCITY);
}

void Game::EffectsConfuse(bool isShow)
{
    Effects->Confuse = isShow;
}

void Game::EffectsChaos(bool isShow)
{
    Effects->Chaos = isShow;
}
