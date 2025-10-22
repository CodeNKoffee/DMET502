#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

// BMP Texture Loading
struct BMPHeader
{
  char signature[2];
  int fileSize;
  int reserved;
  int dataOffset;
  int headerSize;
  int width;
  int height;
  short planes;
  short bitsPerPixel;
  int compression;
  int imageSize;
  int xPixelsPerMeter;
  int yPixelsPerMeter;
  int colorsUsed;
  int importantColors;
};

GLuint loadBMPTexture(const char *filename)
{
  FILE *file = fopen(filename, "rb");
  if (!file)
  {
    printf("Could not open texture file: %s\n", filename);
    return 0;
  }

  BMPHeader header;
  fread(&header, sizeof(BMPHeader), 1, file);

  if (header.signature[0] != 'B' || header.signature[1] != 'M')
  {
    printf("Invalid BMP file: %s\n", filename);
    fclose(file);
    return 0;
  }

  if (header.bitsPerPixel != 24)
  {
    printf("Only 24-bit BMP files supported: %s\n", filename);
    fclose(file);
    return 0;
  }

  fseek(file, header.dataOffset, SEEK_SET);

  int width = header.width;
  int height = header.height;
  int bytesPerPixel = 3;
  int rowSize = ((width * bytesPerPixel + 3) / 4) * 4;

  unsigned char *imageData = new unsigned char[rowSize * height];
  fread(imageData, 1, rowSize * height, file);
  fclose(file);

  // Flip image vertically (BMP is bottom-up)
  unsigned char *flippedData = new unsigned char[rowSize * height];
  for (int y = 0; y < height; y++)
  {
    memcpy(flippedData + y * rowSize,
           imageData + (height - 1 - y) * rowSize,
           rowSize);
  }

  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, flippedData);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  delete[] imageData;
  delete[] flippedData;

  return textureID;
}

// Create simple colored textures programmatically
GLuint createColorTexture(float r, float g, float b)
{
  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  unsigned char color[3] = {(unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255)};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, color);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  return textureID;
}

// Data Structures
struct GameObject
{
  float x, y;
  float width, height;
  bool active;
  int type;
  float rotation;
};

struct PowerUp
{
  float x, y;
  bool active;
  float animScale;
  int type;
};

// Global Variables
const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 600;
const int TOP_PANEL_HEIGHT = 100;
const int BOTTOM_PANEL_HEIGHT = 100;
const int GAME_AREA_TOP = WINDOW_HEIGHT - TOP_PANEL_HEIGHT;
const int GAME_AREA_BOTTOM = BOTTOM_PANEL_HEIGHT;

// Game State
enum GameState
{
  SETUP,
  RUNNING,
  WIN,
  LOSE
};
GameState gameState = SETUP;

// Player
float playerX = 500, playerY = 120;
float playerAngle = 0;
const float PLAYER_SIZE = 20;
const float PLAYER_SPEED = 3.0f;
float currentSpeed = PLAYER_SPEED;

// Game Target (Gate A01)
float planeX = 500, planeY = 480;
float bezierT = 0.0f;
float bezierSpeed = 0.01f;
int bezierP0[2] = {400, 480};
int bezierP1[2] = {600, 480};
int bezierP2[2] = {600, 480};
int bezierP3[2] = {400, 480};

// Game Objects
std::vector<GameObject> obstacles;
std::vector<GameObject> collectibles;
std::vector<PowerUp> powerups;
GameObject friendObj = {300, 300, 25, 25, true, 0, 0};
bool friendCollected = false;

// Game Stats
int score = 0;
int lives = 5;
int gameTime = 60; // seconds
float gameTimer = 0;

// Drawing Mode
enum DrawingMode
{
  NONE,
  OBSTACLE,
  COLLECTIBLE,
  POWERUP1,
  POWERUP2
};
DrawingMode drawingMode = NONE;

// Power-up Effects
bool invincible = false;
float invincibleTimer = 0;
bool speedBoost = false;
float speedBoostTimer = 0;

// Animation Variables
float collectibleRotation = 0;
float conveyorOffset = 0;

// Texture IDs
GLuint playerTexture, planeTexture, guardTexture, boardingPassTexture;
GLuint friendTexture, badgeTexture, fastTrackTexture, luggageTexture, panelTexture;

// Text rendering function (from provided code)
void print(int x, int y, char *string)
{
  int len, i;
  glRasterPos2f(x, y);
  len = (int)strlen(string);
  for (i = 0; i < len; i++)
  {
    glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, string[i]);
  }
}

// Bezier curve function (from provided code)
int *bezier(float t, int *p0, int *p1, int *p2, int *p3)
{
  static int res[2];
  res[0] = pow((1 - t), 3) * p0[0] + 3 * t * pow((1 - t), 2) * p1[0] + 3 * pow(t, 2) * (1 - t) * p2[0] + pow(t, 3) * p3[0];
  res[1] = pow((1 - t), 3) * p0[1] + 3 * t * pow((1 - t), 2) * p1[1] + 3 * pow(t, 2) * (1 - t) * p2[1] + pow(t, 3) * p3[1];
  return res;
}

// Drawing Functions
void drawPlayer(float x, float y, float angle)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glRotatef(angle, 0, 0, 1);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, playerTexture);

  // Body/Suitcase (GL_POLYGON)
  glColor3f(1.0f, 1.0f, 1.0f);
  glBegin(GL_POLYGON);
  glTexCoord2f(0, 0);
  glVertex2f(-10, -8);
  glTexCoord2f(1, 0);
  glVertex2f(10, -8);
  glTexCoord2f(1, 1);
  glVertex2f(8, 8);
  glTexCoord2f(0, 1);
  glVertex2f(-8, 8);
  glEnd();

  // Arms/Legs (GL_TRIANGLES)
  glColor3f(0.8f, 0.6f, 0.4f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-12, 0);
  glVertex2f(-8, 4);
  glVertex2f(-8, -4);
  glVertex2f(12, 0);
  glVertex2f(8, 4);
  glVertex2f(8, -4);
  glEnd();

  // Luggage Handle (GL_LINE_LOOP)
  glColor3f(0.3f, 0.3f, 0.3f);
  glLineWidth(2);
  glBegin(GL_LINE_LOOP);
  glVertex2f(-6, 8);
  glVertex2f(6, 8);
  glVertex2f(6, 12);
  glVertex2f(-6, 12);
  glEnd();

  // Face Details (GL_POINTS)
  glColor3f(0.0f, 0.0f, 0.0f);
  glPointSize(3);
  glBegin(GL_POINTS);
  glVertex2f(-3, 2);
  glVertex2f(3, 2);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawPlane(float x, float y)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, planeTexture);

  // Fuselage (GL_POLYGON)
  glColor3f(0.9f, 0.9f, 0.9f);
  glBegin(GL_POLYGON);
  glTexCoord2f(0, 0);
  glVertex2f(-25, -5);
  glTexCoord2f(1, 0);
  glVertex2f(25, -5);
  glTexCoord2f(1, 1);
  glVertex2f(20, 5);
  glTexCoord2f(0, 1);
  glVertex2f(-20, 5);
  glEnd();

  // Wings (GL_TRIANGLES)
  glColor3f(0.7f, 0.7f, 0.7f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-15, 0);
  glVertex2f(-25, -15);
  glVertex2f(-5, -15);
  glVertex2f(15, 0);
  glVertex2f(25, -15);
  glVertex2f(5, -15);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawGuard(float x, float y)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, guardTexture);

  // Body (GL_QUADS)
  glColor3f(0.2f, 0.2f, 0.8f);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(-8, -12);
  glTexCoord2f(1, 0);
  glVertex2f(8, -12);
  glTexCoord2f(1, 1);
  glVertex2f(8, 8);
  glTexCoord2f(0, 1);
  glVertex2f(-8, 8);
  glEnd();

  // Hat (GL_TRIANGLES)
  glColor3f(0.1f, 0.1f, 0.1f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-10, 8);
  glVertex2f(0, 15);
  glVertex2f(10, 8);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawBoardingPass(float x, float y, float rotation)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glRotatef(rotation, 0, 0, 1);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, boardingPassTexture);

  // Paper (GL_QUADS)
  glColor3f(1.0f, 1.0f, 0.9f);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(-6, -4);
  glTexCoord2f(1, 0);
  glVertex2f(6, -4);
  glTexCoord2f(1, 1);
  glVertex2f(6, 4);
  glTexCoord2f(0, 1);
  glVertex2f(-6, 4);
  glEnd();

  // Text Lines (GL_LINES)
  glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1);
  glBegin(GL_LINES);
  glVertex2f(-4, 2);
  glVertex2f(4, 2);
  glVertex2f(-4, 0);
  glVertex2f(4, 0);
  glVertex2f(-4, -2);
  glVertex2f(4, -2);
  glEnd();

  // Stamps (GL_POINTS)
  glColor3f(1.0f, 0.0f, 0.0f);
  glPointSize(4);
  glBegin(GL_POINTS);
  glVertex2f(4, 3);
  glVertex2f(-4, -3);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawFriend(float x, float y)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, friendTexture);

  // Body (GL_POLYGON)
  glColor3f(0.8f, 0.6f, 0.4f);
  glBegin(GL_POLYGON);
  glTexCoord2f(0, 0);
  glVertex2f(-12, -15);
  glTexCoord2f(1, 0);
  glVertex2f(12, -15);
  glTexCoord2f(1, 1);
  glVertex2f(8, 10);
  glTexCoord2f(0, 1);
  glVertex2f(-8, 10);
  glEnd();

  // Arms (GL_TRIANGLES)
  glColor3f(0.7f, 0.5f, 0.3f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-15, 0);
  glVertex2f(-20, 5);
  glVertex2f(-10, 5);
  glVertex2f(15, 0);
  glVertex2f(20, 5);
  glVertex2f(10, 5);
  glEnd();

  // Face (GL_POINTS)
  glColor3f(0.0f, 0.0f, 0.0f);
  glPointSize(3);
  glBegin(GL_POINTS);
  glVertex2f(-4, 5);
  glVertex2f(4, 5);
  glVertex2f(0, 2);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawManagerBadge(float x, float y, float scale)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glScalef(scale, scale, 1);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, badgeTexture);

  // Badge Shape (GL_POLYGON)
  glColor3f(1.0f, 0.8f, 0.0f);
  glBegin(GL_POLYGON);
  glTexCoord2f(0, 0);
  glVertex2f(-8, -6);
  glTexCoord2f(1, 0);
  glVertex2f(8, -6);
  glTexCoord2f(1, 1);
  glVertex2f(6, 6);
  glTexCoord2f(0, 1);
  glVertex2f(-6, 6);
  glEnd();

  // Border (GL_LINE_LOOP)
  glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(2);
  glBegin(GL_LINE_LOOP);
  glVertex2f(-8, -6);
  glVertex2f(8, -6);
  glVertex2f(6, 6);
  glVertex2f(-6, 6);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawFastTrackPass(float x, float y, float scale)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glScalef(scale, scale, 1);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, fastTrackTexture);

  // Card (GL_QUADS)
  glColor3f(0.0f, 0.8f, 0.0f);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(-8, -5);
  glTexCoord2f(1, 0);
  glVertex2f(8, -5);
  glTexCoord2f(1, 1);
  glVertex2f(8, 5);
  glTexCoord2f(0, 1);
  glVertex2f(-8, 5);
  glEnd();

  // Arrow Symbol (GL_TRIANGLES)
  glColor3f(1.0f, 1.0f, 1.0f);
  glBegin(GL_TRIANGLES);
  glVertex2f(0, 0);
  glVertex2f(-4, -3);
  glVertex2f(-4, 3);
  glVertex2f(0, 0);
  glVertex2f(4, -3);
  glVertex2f(4, 3);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawStressIndicator(float x, float y, bool filled)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  // Apply texture
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, luggageTexture);

  // Outer Luggage (GL_QUADS)
  glColor3f(0.6f, 0.4f, 0.2f);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(-8, -6);
  glTexCoord2f(1, 0);
  glVertex2f(8, -6);
  glTexCoord2f(1, 1);
  glVertex2f(8, 6);
  glTexCoord2f(0, 1);
  glVertex2f(-8, 6);
  glEnd();

  // Inner Fill (GL_POLYGON)
  if (filled)
  {
    glColor3f(0.0f, 1.0f, 0.0f);
  }
  else
  {
    glColor3f(0.3f, 0.3f, 0.3f);
  }
  glBegin(GL_POLYGON);
  glVertex2f(-6, -4);
  glVertex2f(6, -4);
  glVertex2f(6, 4);
  glVertex2f(-6, 4);
  glEnd();

  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void drawConveyorBelt()
{
  // Moving background animation
  glColor3f(0.4f, 0.4f, 0.4f);
  glBegin(GL_QUADS);
  glVertex2f(0, GAME_AREA_BOTTOM);
  glVertex2f(WINDOW_WIDTH, GAME_AREA_BOTTOM);
  glVertex2f(WINDOW_WIDTH, GAME_AREA_TOP);
  glVertex2f(0, GAME_AREA_TOP);
  glEnd();

  // Moving luggage rectangles
  glColor3f(0.5f, 0.3f, 0.1f);
  for (int i = 0; i < 5; i++)
  {
    float x = fmod(conveyorOffset + i * 200, WINDOW_WIDTH + 50) - 25;
    glBegin(GL_QUADS);
    glVertex2f(x, GAME_AREA_BOTTOM + 20);
    glVertex2f(x + 40, GAME_AREA_BOTTOM + 20);
    glVertex2f(x + 40, GAME_AREA_BOTTOM + 40);
    glVertex2f(x, GAME_AREA_BOTTOM + 40);
    glEnd();
  }
}

// Collision Detection
bool checkCollision(float x1, float y1, float w1, float h1,
                    float x2, float y2, float w2, float h2)
{
  return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

void handleCollisions()
{
  // Check player vs obstacles
  for (auto &obstacle : obstacles)
  {
    if (obstacle.active && checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                                          PLAYER_SIZE, PLAYER_SIZE,
                                          obstacle.x - obstacle.width / 2, obstacle.y - obstacle.height / 2,
                                          obstacle.width, obstacle.height))
    {
      if (!invincible)
      {
        lives--;
        // Block movement by moving player back
        playerX = std::max(playerX, obstacle.x + obstacle.width / 2 + PLAYER_SIZE / 2);
        playerX = std::min(playerX, obstacle.x - obstacle.width / 2 - PLAYER_SIZE / 2);
        playerY = std::max(playerY, obstacle.y + obstacle.height / 2 + PLAYER_SIZE / 2);
        playerY = std::min(playerY, obstacle.y - obstacle.height / 2 - PLAYER_SIZE / 2);
      }
    }
  }

  // Check player vs collectibles
  for (auto it = collectibles.begin(); it != collectibles.end();)
  {
    if (it->active && checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                                     PLAYER_SIZE, PLAYER_SIZE,
                                     it->x - it->width / 2, it->y - it->height / 2,
                                     it->width, it->height))
    {
      score += 5;
      it = collectibles.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Check player vs friend
  if (friendObj.active && !friendCollected &&
      checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                     PLAYER_SIZE, PLAYER_SIZE,
                     friendObj.x - friendObj.width / 2, friendObj.y - friendObj.height / 2,
                     friendObj.width, friendObj.height))
  {
    friendCollected = true;
    friendObj.active = false;
  }

  // Check player vs powerups
  for (auto it = powerups.begin(); it != powerups.end();)
  {
    if (it->active && checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                                     PLAYER_SIZE, PLAYER_SIZE,
                                     it->x - 10, it->y - 10, 20, 20))
    {
      if (it->type == 1)
      { // Manager Approval
        invincible = true;
        invincibleTimer = 5.0f;
      }
      else if (it->type == 2)
      { // Fast Track
        speedBoost = true;
        speedBoostTimer = 5.0f;
      }
      it = powerups.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // Check win condition
  if (friendCollected && checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                                        PLAYER_SIZE, PLAYER_SIZE,
                                        planeX - 25, planeY - 5, 50, 10))
  {
    gameState = WIN;
  }
}

// Game Functions
void init()
{
  // Create textures (using solid colors since we don't have actual BMP files)
  playerTexture = createColorTexture(0.8f, 0.6f, 0.4f);
  planeTexture = createColorTexture(0.9f, 0.9f, 0.9f);
  guardTexture = createColorTexture(0.2f, 0.2f, 0.8f);
  boardingPassTexture = createColorTexture(1.0f, 1.0f, 0.9f);
  friendTexture = createColorTexture(0.8f, 0.6f, 0.4f);
  badgeTexture = createColorTexture(1.0f, 0.8f, 0.0f);
  fastTrackTexture = createColorTexture(0.0f, 0.8f, 0.0f);
  luggageTexture = createColorTexture(0.6f, 0.4f, 0.2f);
  panelTexture = createColorTexture(0.3f, 0.3f, 0.3f);

  // Initialize OpenGL
  glEnable(GL_TEXTURE_2D);
  glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

  // Initialize game state
  playerX = 500;
  playerY = 120;
  playerAngle = 0;
  gameState = SETUP;
  score = 0;
  lives = 5;
  gameTime = 60;
  gameTimer = 0;
  friendCollected = false;

  // Clear vectors
  obstacles.clear();
  collectibles.clear();
  powerups.clear();
}

void display()
{
  glClear(GL_COLOR_BUFFER_BIT);

  // Draw top panel
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, panelTexture);
  glColor3f(0.2f, 0.2f, 0.3f);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(0, GAME_AREA_TOP);
  glTexCoord2f(1, 0);
  glVertex2f(WINDOW_WIDTH, GAME_AREA_TOP);
  glTexCoord2f(1, 1);
  glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
  glTexCoord2f(0, 1);
  glVertex2f(0, WINDOW_HEIGHT);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  // Draw health indicators
  for (int i = 0; i < 5; i++)
  {
    drawStressIndicator(50 + i * 40, WINDOW_HEIGHT - 50, i < lives);
  }

  // Draw score and time
  glColor3f(1.0f, 1.0f, 1.0f);
  char scoreText[50];
  sprintf(scoreText, "Score: %d", score);
  print(200, WINDOW_HEIGHT - 30, scoreText);

  char timeText[50];
  sprintf(timeText, "Time: %d", gameTime);
  print(400, WINDOW_HEIGHT - 30, timeText);

  if (gameState == SETUP)
  {
    char setupText[50] = "Click objects below, then press R to start";
    print(600, WINDOW_HEIGHT - 30, setupText);
  }

  // Draw game area background
  drawConveyorBelt();

  // Draw all game objects
  for (const auto &obstacle : obstacles)
  {
    if (obstacle.active)
    {
      drawGuard(obstacle.x, obstacle.y);
    }
  }

  for (const auto &collectible : collectibles)
  {
    if (collectible.active)
    {
      drawBoardingPass(collectible.x, collectible.y, collectible.rotation);
    }
  }

  for (const auto &powerup : powerups)
  {
    if (powerup.active)
    {
      if (powerup.type == 1)
      {
        drawManagerBadge(powerup.x, powerup.y, powerup.animScale);
      }
      else
      {
        drawFastTrackPass(powerup.x, powerup.y, powerup.animScale);
      }
    }
  }

  // Draw friend if not collected
  if (friendObj.active && !friendCollected)
  {
    drawFriend(friendObj.x, friendObj.y);
  }

  // Draw player
  drawPlayer(playerX, playerY, playerAngle);

  // Draw plane (game target)
  drawPlane(planeX, planeY);

  // Draw bottom panel
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, panelTexture);
  glColor3f(0.2f, 0.2f, 0.3f);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(0, 0);
  glTexCoord2f(1, 0);
  glVertex2f(WINDOW_WIDTH, 0);
  glTexCoord2f(1, 1);
  glVertex2f(WINDOW_WIDTH, BOTTOM_PANEL_HEIGHT);
  glTexCoord2f(0, 1);
  glVertex2f(0, BOTTOM_PANEL_HEIGHT);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  // Draw template objects in bottom panel
  drawGuard(100, 50);
  drawBoardingPass(250, 50, 0);
  drawManagerBadge(400, 50, 1.0f);
  drawFastTrackPass(550, 50, 1.0f);

  // Draw labels
  glColor3f(1.0f, 1.0f, 1.0f);
  print(80, 20, (char *)"Guard");
  print(220, 20, (char *)"Pass");
  print(360, 20, (char *)"Badge");
  print(520, 20, (char *)"Fast");

  // Draw game end screens
  if (gameState == WIN)
  {
    glColor3f(0.0f, 1.0f, 0.0f);
    char winText[100];
    sprintf(winText, "GAME WIN! Final Score: %d", score);
    print(350, 300, winText);
  }
  else if (gameState == LOSE)
  {
    glColor3f(1.0f, 0.0f, 0.0f);
    char loseText[100];
    sprintf(loseText, "GAME LOSE! Final Score: %d", score);
    print(350, 300, loseText);
  }

  glFlush();
}

void timer(int value)
{
  if (gameState == RUNNING)
  {
    // Update game timer
    gameTimer += 1.0f / 60.0f;
    if (gameTimer >= 1.0f)
    {
      gameTimer = 0;
      gameTime--;
      if (gameTime <= 0)
      {
        gameState = LOSE;
      }
    }

    // Update power-up timers
    if (invincible)
    {
      invincibleTimer -= 1.0f / 60.0f;
      if (invincibleTimer <= 0)
      {
        invincible = false;
      }
    }

    if (speedBoost)
    {
      speedBoostTimer -= 1.0f / 60.0f;
      if (speedBoostTimer <= 0)
      {
        speedBoost = false;
        currentSpeed = PLAYER_SPEED;
      }
    }

    // Update animations
    bezierT += bezierSpeed;
    if (bezierT > 1.0f)
      bezierT = 0.0f;

    int *planePos = bezier(bezierT, bezierP0, bezierP1, bezierP2, bezierP3);
    planeX = planePos[0];

    collectibleRotation += 2.0f;
    if (collectibleRotation >= 360.0f)
      collectibleRotation = 0.0f;

    conveyorOffset += 1.0f;
    if (conveyorOffset >= WINDOW_WIDTH + 50)
      conveyorOffset = 0;

    // Update collectible rotations
    for (auto &collectible : collectibles)
    {
      collectible.rotation = collectibleRotation;
    }

    // Update power-up scales
    for (auto &powerup : powerups)
    {
      powerup.animScale = 0.8f + 0.4f * sin(glutGet(GLUT_ELAPSED_TIME) * 0.01f);
    }

    // Check collisions
    handleCollisions();

    // Check lose condition
    if (lives <= 0)
    {
      gameState = LOSE;
    }
  }

  glutPostRedisplay();
  glutTimerFunc(16, timer, 0); // ~60 FPS
}

void keyboard(unsigned char key, int x, int y)
{
  if (gameState == SETUP && (key == 'r' || key == 'R'))
  {
    gameState = RUNNING;
    return;
  }

  if (gameState != RUNNING)
    return;

  float newX = playerX;
  float newY = playerY;

  switch (key)
  {
  case 'w':
  case 'W':
    newY += currentSpeed;
    playerAngle = 90;
    break;
  case 's':
  case 'S':
    newY -= currentSpeed;
    playerAngle = 270;
    break;
  case 'a':
  case 'A':
    newX -= currentSpeed;
    playerAngle = 180;
    break;
  case 'd':
  case 'D':
    newX += currentSpeed;
    playerAngle = 0;
    break;
  }

  // Boundary checking
  if (newX >= PLAYER_SIZE / 2 && newX <= WINDOW_WIDTH - PLAYER_SIZE / 2 &&
      newY >= GAME_AREA_BOTTOM + PLAYER_SIZE / 2 && newY <= GAME_AREA_TOP - PLAYER_SIZE / 2)
  {
    playerX = newX;
    playerY = newY;
  }
}

void specialKeys(int key, int x, int y)
{
  if (gameState != RUNNING)
    return;

  float newX = playerX;
  float newY = playerY;

  switch (key)
  {
  case GLUT_KEY_UP:
    newY += currentSpeed;
    playerAngle = 90;
    break;
  case GLUT_KEY_DOWN:
    newY -= currentSpeed;
    playerAngle = 270;
    break;
  case GLUT_KEY_LEFT:
    newX -= currentSpeed;
    playerAngle = 180;
    break;
  case GLUT_KEY_RIGHT:
    newX += currentSpeed;
    playerAngle = 0;
    break;
  }

  // Boundary checking
  if (newX >= PLAYER_SIZE / 2 && newX <= WINDOW_WIDTH - PLAYER_SIZE / 2 &&
      newY >= GAME_AREA_BOTTOM + PLAYER_SIZE / 2 && newY <= GAME_AREA_TOP - PLAYER_SIZE / 2)
  {
    playerX = newX;
    playerY = newY;
  }
}

void mouse(int button, int state, int x, int y)
{
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
  {
    y = WINDOW_HEIGHT - y; // Convert y coordinate

    // Check if clicking in bottom panel (object selection)
    if (y < BOTTOM_PANEL_HEIGHT)
    {
      if (x >= 60 && x <= 140)
      {
        drawingMode = OBSTACLE;
      }
      else if (x >= 210 && x <= 290)
      {
        drawingMode = COLLECTIBLE;
      }
      else if (x >= 360 && x <= 440)
      {
        drawingMode = POWERUP1;
      }
      else if (x >= 510 && x <= 590)
      {
        drawingMode = POWERUP2;
      }
      return;
    }

    // Check if clicking in game area (object placement)
    if (y >= GAME_AREA_BOTTOM && y <= GAME_AREA_TOP && drawingMode != NONE)
    {
      bool canPlace = true;

      // Check for overlaps
      for (const auto &obstacle : obstacles)
      {
        if (obstacle.active && checkCollision(x - 10, y - 10, 20, 20,
                                              obstacle.x - obstacle.width / 2, obstacle.y - obstacle.height / 2,
                                              obstacle.width, obstacle.height))
        {
          canPlace = false;
          break;
        }
      }

      for (const auto &collectible : collectibles)
      {
        if (collectible.active && checkCollision(x - 10, y - 10, 20, 20,
                                                 collectible.x - collectible.width / 2, collectible.y - collectible.height / 2,
                                                 collectible.width, collectible.height))
        {
          canPlace = false;
          break;
        }
      }

      for (const auto &powerup : powerups)
      {
        if (powerup.active && checkCollision(x - 10, y - 10, 20, 20,
                                             powerup.x - 10, powerup.y - 10, 20, 20))
        {
          canPlace = false;
          break;
        }
      }

      if (canPlace)
      {
        switch (drawingMode)
        {
        case OBSTACLE:
          obstacles.push_back({(float)x, (float)y, 16, 20, true, 0, 0});
          break;
        case COLLECTIBLE:
          collectibles.push_back({(float)x, (float)y, 12, 8, true, 0, 0});
          break;
        case POWERUP1:
          powerups.push_back({(float)x, (float)y, true, 1.0f, 1});
          break;
        case POWERUP2:
          powerups.push_back({(float)x, (float)y, true, 1.0f, 2});
          break;
        case NONE:
          break;
        }
      }
    }
  }
}

int main(int argc, char **argv)
{
  glutInit(&argc, argv);
  glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
  glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
  glutCreateWindow("Airport Rush: Cluj-Napoca Last-Minute Boarding");

  init();

  glutDisplayFunc(display);
  glutKeyboardFunc(keyboard);
  glutSpecialFunc(specialKeys);
  glutMouseFunc(mouse);
  glutTimerFunc(16, timer, 0);

  gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);

  glutMainLoop();
  return 0;
}
