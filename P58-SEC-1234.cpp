#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <unistd.h> 

// ROMANIA FLAG COLORS
#define ROMANIA_BLUE_R 0.0f
#define ROMANIA_BLUE_G 0.169f
#define ROMANIA_BLUE_B 0.498f

#define ROMANIA_YELLOW_R 0.988f
#define ROMANIA_YELLOW_G 0.820f
#define ROMANIA_YELLOW_B 0.086f

#define ROMANIA_RED_R 0.808f
#define ROMANIA_RED_G 0.067f
#define ROMANIA_RED_B 0.149f

// --- BMP Texture Loading ---

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

GLuint loadBMPTexture(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    printf("ERROR: Could not open texture file: %s\n", filename);
    printf("Current working directory: ");
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("%s\n", cwd);
    } else {
      printf("Unknown (getcwd failed)\n");
    }
    return 0;
  }

  char signature[2];
  int fileSize, reserved, dataOffset, headerSize, width, height;
  short planes, bitsPerPixel;
  int compression, imageSize, xPixelsPerMeter, yPixelsPerMeter, colorsUsed, importantColors;

  fread(signature, sizeof(char), 2, file);
  fread(&fileSize, sizeof(int), 1, file);
  fread(&reserved, sizeof(int), 1, file);
  fread(&dataOffset, sizeof(int), 1, file);
  fread(&headerSize, sizeof(int), 1, file);
  fread(&width, sizeof(int), 1, file);
  fread(&height, sizeof(int), 1, file);
  fread(&planes, sizeof(short), 1, file);
  fread(&bitsPerPixel, sizeof(short), 1, file);
  fread(&compression, sizeof(int), 1, file);
  fread(&imageSize, sizeof(int), 1, file);
  fread(&xPixelsPerMeter, sizeof(int), 1, file);
  fread(&yPixelsPerMeter, sizeof(int), 1, file);
  fread(&colorsUsed, sizeof(int), 1, file);
  fread(&importantColors, sizeof(int), 1, file);

  printf("DEBUG: Signature: %c%c, Width: %d, Height: %d, BitsPerPixel: %d, Compression: %d\n",
         signature[0], signature[1], width, height, bitsPerPixel, compression);

  if (signature[0] != 'B' || signature[1] != 'M') {
    printf("ERROR: Invalid BMP file signature: %s\n", filename);
    fclose(file);
    return 0;
  }

  if (bitsPerPixel != 24) {
    printf("ERROR: Only 24-bit BMP files supported. This file has %d bits per pixel.\n", bitsPerPixel);
    fclose(file);
    return 0;
  }

  if (compression != 0) {
    printf("ERROR: Only uncompressed BMP files supported. This file has compression type %d.\n", compression);
    fclose(file);
    return 0;
  }

  int dataSize = width * height * 3;
  unsigned char *imageData = (unsigned char *)malloc(dataSize);
  if (!imageData) {
    printf("ERROR: Could not allocate memory for image data.\n");
    fclose(file);
    return 0;
  }

  fseek(file, dataOffset, SEEK_SET);
  fread(imageData, 1, dataSize, file);
  fclose(file);

  for (int i = 0; i < height / 2; i++) {
    for (int j = 0; j < width * 3; j++) {
      unsigned char temp = imageData[i * width * 3 + j];
      imageData[i * width * 3 + j] = imageData[(height - 1 - i) * width * 3 + j];
      imageData[(height - 1 - i) * width * 3 + j] = temp;
    }
  }

  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, imageData);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    printf("OpenGL Error after texture setup: %d\n", err);
  }

  free(imageData);
  printf("SUCCESS: Loaded Texture ID %d: %s (Width: %d, Height: %d)\n",
         textureID, filename, width, height);
  return textureID;
}

// --- Data Structures ---

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

// --- Global Variables ---

const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 600;
const int TOP_PANEL_HEIGHT = 100;
const int BOTTOM_PANEL_HEIGHT = 100;
const int GAME_AREA_TOP = WINDOW_HEIGHT - TOP_PANEL_HEIGHT;
const int GAME_AREA_BOTTOM = BOTTOM_PANEL_HEIGHT;

float cameraOffsetX = 0;
float cameraOffsetY = 0;

enum GameState
{
  SETUP,
  RUNNING,
  WIN,
  LOSE
};
GameState gameState = SETUP;

float playerX = 500, playerY = 50;
float playerAngle = 0;
const float PLAYER_SIZE = 20;
const float PLAYER_SPEED = 3.0f;
float currentSpeed = PLAYER_SPEED;

float planeX = 500, planeY = 450;
float bezierT = 0.0f;
float bezierSpeed = 0.01f;
int bezierP0[2] = {400, 450};
int bezierP1[2] = {500, 450};
int bezierP2[2] = {600, 450};
int bezierP3[2] = {500, 450};

std::vector<GameObject> obstacles;
std::vector<GameObject> collectibles;
std::vector<PowerUp> powerups;
GameObject friendObj = {487, 400, 30, 35, true, 0, 0};
bool friendCollected = false;

int score = 0;
int lives = 5;
int gameTime = 60;
float gameTimer = 0;

enum DrawingMode
{
  NONE,
  OBSTACLE,
  COLLECTIBLE,
  POWERUP1,
  POWERUP2
};
DrawingMode drawingMode = NONE;

bool invincible = false;
float invincibleTimer = 0;
bool speedBoost = false;
float speedBoostTimer = 0;

float collectibleRotation = 0;
float conveyorOffset = 0;

GLuint mapTexture;
GLuint playerTexture, planeTexture, guardTexture, boardingPassTexture;
GLuint friendTexture, badgeTexture, fastTrackTexture, luggageTexture, panelTexture;

bool checkCollision(float x1, float y1, float w1, float h1,
                    float x2, float y2, float w2, float h2);

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

int* bezier(float t, int* p0, int* p1, int* p2, int* p3)
{
  static int res[2];
  res[0] = pow((1-t),3)*p0[0]+3*t*pow((1-t),2)*p1[0]+3*pow(t,2)*(1-t)*p2[0]+pow(t,3)*p3[0];
  res[1] = pow((1-t),3)*p0[1]+3*t*pow((1-t),2)*p1[1]+3*pow(t,2)*(1-t)*p2[1]+pow(t,3)*p3[1];
  return res;
}

// --- DRAWING FUNCTIONS ---

void drawPlayer(float x, float y, float angle)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glRotatef(angle, 0, 0, 1);

  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(5 * cosf(theta), 8 + 5 * sinf(theta));
  }
  glEnd();

  glColor3f(0.1f, 0.1f, 0.15f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(6 * cosf(theta), 9 + 6 * sinf(theta));
  }
  glEnd();

  glColor3f(0.4f, 0.4f, 0.5f);
  glBegin(GL_POLYGON);
  glVertex2f(-8, 3);
  glVertex2f(8, 3);
  glVertex2f(10, -8);
  glVertex2f(-10, -8);
  glEnd();

  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glLineWidth(2);
  glBegin(GL_LINES);
  glVertex2f(0, 3);
  glVertex2f(0, -5);
  glEnd();

  glColor3f(0.2f, 0.2f, 0.25f);
  glBegin(GL_QUADS);
  glVertex2f(-10, -8);
  glVertex2f(10, -8);
  glVertex2f(8, -15);
  glVertex2f(-8, -15);
  glEnd();

  glColor3f(0.7f, 0.5f, 0.3f);
  glBegin(GL_QUADS);
  glVertex2f(10, 0);
  glVertex2f(16, 0);
  glVertex2f(16, -10);
  glVertex2f(10, -10);
  glEnd();

  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_QUADS);
  glVertex2f(13, -8);
  glVertex2f(15, -8);
  glVertex2f(15, -6);
  glVertex2f(13, -6);
  glEnd();

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glLineWidth(3);
  glBegin(GL_LINE_STRIP);
  glVertex2f(13, 0);
  glVertex2f(13, 3);
  glEnd();

  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-8, 2);
  glVertex2f(-12, 0);
  glVertex2f(-8, -2);
  glEnd();

  glColor3f(0.0f, 0.0f, 0.0f);
  glPointSize(3);
  glBegin(GL_POINTS);
  glVertex2f(-2, 9);
  glVertex2f(2, 9);
  glEnd();

  glPopMatrix();
}

void drawPlane(float x, float y)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  glColor3f(0.95f, 0.95f, 0.98f);
  glBegin(GL_POLYGON);
  glVertex2f(-30, -6);
  glVertex2f(25, -6);
  glVertex2f(30, -3);
  glVertex2f(30, 3);
  glVertex2f(25, 6);
  glVertex2f(-30, 6);
  glVertex2f(-35, 3);
  glVertex2f(-35, -3);
  glEnd();

  glColor3f(0.85f, 0.85f, 0.88f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-20, 0);
  glVertex2f(-35, -18);
  glVertex2f(-10, -18);
  glVertex2f(20, 0);
  glVertex2f(35, -18);
  glVertex2f(10, -18);
  glEnd();

  float tailWidth = 10;
  float tailHeight = 12;

  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_QUADS);
  glVertex2f(-30, 0);
  glVertex2f(-30 - tailWidth / 3, 0);
  glVertex2f(-30 - tailWidth / 3, tailHeight);
  glVertex2f(-30, tailHeight);
  glEnd();

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_QUADS);
  glVertex2f(-30 - tailWidth / 3, 0);
  glVertex2f(-30 - 2 * tailWidth / 3, 0);
  glVertex2f(-30 - 2 * tailWidth / 3, tailHeight);
  glVertex2f(-30 - tailWidth / 3, tailHeight);
  glEnd();

  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_QUADS);
  glVertex2f(-30 - 2 * tailWidth / 3, 0);
  glVertex2f(-30 - tailWidth, 0);
  glVertex2f(-30 - tailWidth, tailHeight);
  glVertex2f(-30 - 2 * tailWidth / 3, tailHeight);
  glEnd();

  glColor3f(0.4f, 0.6f, 0.8f);
  glPointSize(4);
  glBegin(GL_POINTS);
  for (int i = -20; i < 20; i += 8)
  {
    glVertex2f(i, 2);
  }
  glEnd();

  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glRasterPos2f(-15, -2);
  char gateText[] = "A01";
  for (int i = 0; i < 3; i++)
  {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, gateText[i]);
  }

  glPopMatrix();
}

void drawGuard(float x, float y)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(4 * cosf(theta), 10 + 4 * sinf(theta));
  }
  glEnd();

  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_POLYGON);
  for (int i = 10; i < 30; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(5 * cosf(theta), 11 + 4.5f * sinf(theta));
  }
  glEnd();

  glColor3f(0.1f, 0.1f, 0.2f);
  glBegin(GL_QUADS);
  glVertex2f(-6, 10);
  glVertex2f(6, 10);
  glVertex2f(7, 8);
  glVertex2f(-7, 8);
  glEnd();

  glColor3f(0.1f, 0.1f, 0.3f);
  glBegin(GL_QUADS);
  glVertex2f(-7, 7);
  glVertex2f(7, 7);
  glVertex2f(7, -12);
  glVertex2f(-7, -12);
  glEnd();

  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_TRIANGLES);
  glVertex2f(-1, 5);
  glVertex2f(1, 5);
  glVertex2f(0, 0);
  glEnd();

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 6; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 6.0f;
    glVertex2f(-4 + 2 * cosf(theta), 2 + 2 * sinf(theta));
  }
  glEnd();

  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_QUADS);
  glVertex2f(-7, 5);
  glVertex2f(-10, 3);
  glVertex2f(-10, -3);
  glVertex2f(-7, -1);
  glEnd();

  glColor3f(0.2f, 0.2f, 0.2f);
  glBegin(GL_QUADS);
  glVertex2f(7, 0);
  glVertex2f(12, -2);
  glVertex2f(12, -4);
  glVertex2f(7, -2);
  glEnd();

  glPopMatrix();
}

void drawBoardingPass(float x, float y, float rotation)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glRotatef(rotation, 0, 0, 1);

  glColor3f(1.0f, 0.98f, 0.95f);
  glBegin(GL_QUADS);
  glVertex2f(-10, -6);
  glVertex2f(10, -6);
  glVertex2f(10, 6);
  glVertex2f(-10, 6);
  glEnd();

  glColor3f(0.7f, 0.7f, 0.7f);
  glLineWidth(1);
  glLineStipple(1, 0xAAAA);
  glEnable(GL_LINE_STIPPLE);
  glBegin(GL_LINES);
  glVertex2f(2, -6);
  glVertex2f(2, 6);
  glEnd();
  glDisable(GL_LINE_STIPPLE);

  float stripeHeight = 2.0f;
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_QUADS);
  glVertex2f(-10, 6 - stripeHeight);
  glVertex2f(2, 6 - stripeHeight);
  glVertex2f(2, 6);
  glVertex2f(-10, 6);
  glEnd();

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_QUADS);
  glVertex2f(-10, 6 - 2 * stripeHeight);
  glVertex2f(2, 6 - 2 * stripeHeight);
  glVertex2f(2, 6 - stripeHeight);
  glVertex2f(-10, 6 - stripeHeight);
  glEnd();

  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_QUADS);
  glVertex2f(-10, 6 - 3 * stripeHeight);
  glVertex2f(2, 6 - 3 * stripeHeight);
  glVertex2f(2, 6 - 2 * stripeHeight);
  glVertex2f(-10, 6 - 2 * stripeHeight);
  glEnd();

  glColor3f(0.0f, 0.0f, 0.0f);
  glRasterPos2f(-8, 3);
  char airportCode[] = "CLJ-OTP";
  for (int i = 0; i < 7; i++)
  {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, airportCode[i]);
  }

  glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(1);
  glBegin(GL_LINES);
  for (int i = -8; i < 0; i += 1)
  {
    if (i % 2 == 0)
    {
      glVertex2f(i, -5);
      glVertex2f(i, -1);
    }
    else
    {
      glVertex2f(i, -5);
      glVertex2f(i, -2);
    }
  }
  glEnd();

  glPopMatrix();
}

void drawFriend(float x, float y)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  // Headscarf covering head and shoulders
  glColor3f(1.0f, 1.0f, 1.0f);
  glBegin(GL_POLYGON);
  glVertex2f(-8, 12);
  glVertex2f(8, 12);
  glVertex2f(10, 2);
  glVertex2f(8, -2);
  glVertex2f(-8, -2);
  glVertex2f(-10, 2);
  glEnd();

  glColor3f(0.9f, 0.9f, 0.9f);
  glBegin(GL_POLYGON);
  glVertex2f(-8, -2);
  glVertex2f(8, -2);
  glVertex2f(6, -8);
  glVertex2f(-6, -8);
  glEnd();

  // FIXED: White circle outline for hijab opening
  glColor3f(1.0f, 1.0f, 1.0f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(5 * cosf(theta), 8 + 5 * sinf(theta));
  }
  glEnd();

  // FIXED: Skin-toned face inside the white circle
  glColor3f(0.92f, 0.84f, 0.70f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(4 * cosf(theta), 8 + 4 * sinf(theta));
  }
  glEnd();

  // Body
  glColor3f(0.3f, 0.4f, 0.6f);
  glBegin(GL_POLYGON);
  glVertex2f(-8, 2);
  glVertex2f(8, 2);
  glVertex2f(9, -8);
  glVertex2f(-9, -8);
  glEnd();

  glColor3f(0.2f, 0.2f, 0.3f);
  glBegin(GL_QUADS);
  glVertex2f(-9, -8);
  glVertex2f(9, -8);
  glVertex2f(8, -15);
  glVertex2f(-8, -15);
  glEnd();

  glColor3f(0.4f, 0.4f, 0.4f);
  glBegin(GL_QUADS);
  glVertex2f(-12, 0);
  glVertex2f(-8, 0);
  glVertex2f(-8, -6);
  glVertex2f(-12, -6);
  glEnd();

  glColor3f(0.2f, 0.2f, 0.2f);
  glLineWidth(2);
  glBegin(GL_LINES);
  glVertex2f(-10, 0);
  glVertex2f(-6, 2);
  glVertex2f(-10, -3);
  glVertex2f(-6, -1);
  glEnd();

  glColor3f(0.92f, 0.84f, 0.70f);
  glBegin(GL_TRIANGLES);
  glVertex2f(8, 1);
  glVertex2f(12, 4);
  glVertex2f(8, -2);
  glEnd();

  // Eyes
  glColor3f(0.0f, 0.0f, 0.0f);
  glPointSize(2);
  glBegin(GL_POINTS);
  glVertex2f(-2, 9);
  glVertex2f(2, 9);
  glEnd();

  glPopMatrix();
}

void drawManagerBadge(float x, float y, float scale)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glScalef(scale, scale, 1);

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 6; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 6.0f;
    glVertex2f(10 * cosf(theta), 10 * sinf(theta));
  }
  glEnd();

  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(7 * cosf(theta), 7 * sinf(theta));
  }
  glEnd();

  glColor3f(1.0f, 1.0f, 1.0f);
  glRasterPos2f(-6, -2);
  char vipText[] = "VIP";
  for (int i = 0; i < 3; i++)
  {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, vipText[i]);
  }

  glColor3f(0.0f, 0.0f, 0.0f);
  glLineWidth(2);
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < 6; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 6.0f;
    glVertex2f(10 * cosf(theta), 10 * sinf(theta));
  }
  glEnd();

  glPopMatrix();
}

void drawFastTrackPass(float x, float y, float scale)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glScalef(scale, scale, 1);

  float stripWidth = 6.6f;

  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_QUADS);
  glVertex2f(-10, -6);
  glVertex2f(-10 + stripWidth, -6);
  glVertex2f(-10 + stripWidth, 6);
  glVertex2f(-10, 6);
  glEnd();

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_QUADS);
  glVertex2f(-10 + stripWidth, -6);
  glVertex2f(-10 + 2 * stripWidth, -6);
  glVertex2f(-10 + 2 * stripWidth, 6);
  glVertex2f(-10 + stripWidth, 6);
  glEnd();

  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_QUADS);
  glVertex2f(-10 + 2 * stripWidth, -6);
  glVertex2f(10, -6);
  glVertex2f(10, 6);
  glVertex2f(-10 + 2 * stripWidth, 6);
  glEnd();

  glColor3f(1.0f, 1.0f, 1.0f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-6, 0);
  glVertex2f(-2, 3);
  glVertex2f(-2, -3);

  glVertex2f(0, 0);
  glVertex2f(4, 3);
  glVertex2f(4, -3);

  glVertex2f(6, 0);
  glVertex2f(10, 3);
  glVertex2f(10, -3);
  glEnd();

  glPopMatrix();
}

void drawStressIndicator(float x, float y, bool filled)
{
  glPushMatrix();
  glTranslatef(x, y, 0);

  glColor3f(0.4f, 0.3f, 0.2f);
  glBegin(GL_QUADS);
  glVertex2f(-10, -7);
  glVertex2f(10, -7);
  glVertex2f(10, 7);
  glVertex2f(-10, 7);
  glEnd();

  if (filled)
  {
    glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  }
  else
  {
    glColor3f(0.3f, 0.3f, 0.3f);
  }
  glBegin(GL_QUADS);
  glVertex2f(-8, -5);
  glVertex2f(8, -5);
  glVertex2f(8, 5);
  glVertex2f(-8, 5);
  glEnd();

  glColor3f(0.2f, 0.2f, 0.2f);
  glLineWidth(3);
  glBegin(GL_LINE_STRIP);
  glVertex2f(-5, 7);
  glVertex2f(-5, 10);
  glVertex2f(5, 10);
  glVertex2f(5, 7);
  glEnd();

  glPopMatrix();
}

void drawMapBackground() {
  if (mapTexture == 0) {
    printf("DEBUG: mapTexture is 0, using fallback background\n");
    glColor3f(0.55f, 0.55f, 0.58f);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glVertex2f(0, GAME_AREA_BOTTOM);
    glVertex2f(WINDOW_WIDTH, GAME_AREA_BOTTOM);
    glVertex2f(WINDOW_WIDTH, GAME_AREA_TOP);
    glVertex2f(0, GAME_AREA_TOP);
    glEnd();
    return;
  }

  glColor3f(1.0f, 1.0f, 1.0f);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, mapTexture);
  
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f); glVertex2f(0, GAME_AREA_BOTTOM);
  glTexCoord2f(1.0f, 1.0f); glVertex2f(WINDOW_WIDTH, GAME_AREA_BOTTOM);
  glTexCoord2f(1.0f, 0.0f); glVertex2f(WINDOW_WIDTH, GAME_AREA_TOP);
  glTexCoord2f(0.0f, 0.0f); glVertex2f(0, GAME_AREA_TOP);
  glEnd();
  
  glDisable(GL_TEXTURE_2D);
}

bool checkCollision(float x1, float y1, float w1, float h1,
                    float x2, float y2, float w2, float h2)
{
  return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

void handleCollisions()
{
  static int debugCounter = 0;
  if (debugCounter % 60 == 0) {
    printf("DEBUG: Player at (%.1f, %.1f), Friend at (%.1f, %.1f), Collectibles: %zu, Lives: %d, Score: %d\n", 
           playerX, playerY, friendObj.x, friendObj.y, collectibles.size(), lives, score);
  }
  debugCounter++;

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
        printf("DEBUG: Hit guard! Lives: %d\n", lives);
        float dx = playerX - obstacle.x;
        float dy = playerY - obstacle.y;
        if (fabs(dx) > fabs(dy))
        {
          playerX += (dx > 0) ? PLAYER_SPEED * 2 : -PLAYER_SPEED * 2;
        }
        else
        {
          playerY += (dy > 0) ? PLAYER_SPEED * 2 : -PLAYER_SPEED * 2;
        }
      }
    }
  }

  for (auto it = collectibles.begin(); it != collectibles.end();)
  {
    if (it->active && checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                                     PLAYER_SIZE, PLAYER_SIZE,
                                     it->x - it->width / 2, it->y - it->height / 2,
                                     it->width, it->height))
    {
      printf("DEBUG: Collected item at (%.1f, %.1f)\n", it->x, it->y);
      score += 5;
      it = collectibles.erase(it);
    }
    else
    {
      ++it;
    }
  }

  if (friendObj.active && !friendCollected &&
      checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                     PLAYER_SIZE, PLAYER_SIZE,
                     friendObj.x - friendObj.width / 2, friendObj.y - friendObj.height / 2,
                     friendObj.width, friendObj.height))
  {
    printf("DEBUG: Collected friend at (%.1f, %.1f)\n", friendObj.x, friendObj.y);
    friendCollected = true;
    friendObj.active = false;
    score += 20;
  }

  for (auto it = powerups.begin(); it != powerups.end();)
  {
    if (it->active && checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                                     PLAYER_SIZE, PLAYER_SIZE,
                                     it->x - 10, it->y - 10, 20, 20))
    {
      printf("DEBUG: Collected powerup at (%.1f, %.1f)\n", it->x, it->y);
      if (it->type == 1)
      {
        invincible = true;
        invincibleTimer = 5.0f;
        printf("DEBUG: Got VIP badge - invincible for 5 seconds!\n");
      }
      else if (it->type == 2)
      {
        speedBoost = true;
        speedBoostTimer = 5.0f;
        currentSpeed = PLAYER_SPEED * 2.0f;
        printf("DEBUG: Got fast track - speed boost for 5 seconds!\n");
      }
      it = powerups.erase(it);
    }
    else
    {
      ++it;
    }
  }

  if (friendCollected && checkCollision(playerX - PLAYER_SIZE / 2, playerY - PLAYER_SIZE / 2,
                                        PLAYER_SIZE, PLAYER_SIZE,
                                        planeX - 30, planeY - 6, 60, 12))
  {
    printf("DEBUG: Reached plane at (%.1f, %.1f)\n", planeX, planeY);
    gameState = WIN;
  }
}

void init()
{
  printf("DEBUG: Attempting to load texture: cluj-napoca_airport_map.bmp\n");
  mapTexture = loadBMPTexture("./cluj-napoca_airport_map.bmp");
  printf("DEBUG: Texture loaded with ID: %d\n", mapTexture);

  playerTexture = createColorTexture(0.8f, 0.6f, 0.4f);
  planeTexture = createColorTexture(0.9f, 0.9f, 0.9f);
  guardTexture = createColorTexture(0.2f, 0.2f, 0.8f);
  boardingPassTexture = createColorTexture(1.0f, 1.0f, 0.9f);
  friendTexture = createColorTexture(0.8f, 0.6f, 0.4f);
  badgeTexture = createColorTexture(1.0f, 0.8f, 0.0f);
  fastTrackTexture = createColorTexture(0.0f, 0.8f, 0.0f);
  luggageTexture = createColorTexture(0.6f, 0.4f, 0.2f);
  panelTexture = createColorTexture(0.3f, 0.3f, 0.3f);

  glEnable(GL_TEXTURE_2D);
  glClearColor(0.15f, 0.15f, 0.2f, 1.0f);

  cameraOffsetX = 0;
  cameraOffsetY = 0;
  
  playerX = 500;
  playerY = 50;
  playerAngle = 0;
  gameState = SETUP;
  score = 0;
  lives = 5;
  gameTime = 60;
  gameTimer = 0;
  friendCollected = false;

  planeX = 500;
  planeY = 450;
  
  friendObj = {487, 400, 30, 35, true, 0, 0};
  
  cameraOffsetX = 0;
  cameraOffsetY = 250;

  obstacles.clear();
  collectibles.clear();
  powerups.clear();
}

void display()
{
  glClear(GL_COLOR_BUFFER_BIT);

  glPushMatrix();
  glTranslatef(cameraOffsetX, cameraOffsetY, 0);
  drawMapBackground();

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

  if (friendObj.active && !friendCollected)
  {
    drawFriend(friendObj.x, friendObj.y);
  }

  drawPlane(planeX, planeY);
  
  glPopMatrix();
  drawPlayer(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, playerAngle);

  glColor3f(0.1f, 0.1f, 0.15f);
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  glVertex2f(0, GAME_AREA_TOP);
  glVertex2f(WINDOW_WIDTH, GAME_AREA_TOP);
  glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
  glVertex2f(0, WINDOW_HEIGHT);
  glEnd();

  float flagBarHeight = 10;
  glBegin(GL_QUADS);
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glVertex2f(0, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  glVertex2f(0, WINDOW_HEIGHT);
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  glEnd();

  for (int i = 0; i < 5; i++)
  {
    drawStressIndicator(50 + i * 40, WINDOW_HEIGHT - 50, i < lives);
  }

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  char scoreText[50];
  sprintf(scoreText, "SCORE: %d", score);
  print(250, WINDOW_HEIGHT - 60, scoreText);

  char timeText[50];
  sprintf(timeText, "TIME: %d sec", gameTime);
  print(450, WINDOW_HEIGHT - 60, timeText);

  if (friendCollected)
  {
    glColor3f(0.0f, 1.0f, 0.0f);
    print(650, WINDOW_HEIGHT - 60, (char *)"FRIEND: OK!");
  }
  else
  {
    glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
    print(650, WINDOW_HEIGHT - 60, (char *)"FIND FRIEND!");
  }

  if (gameState == SETUP)
  {
    glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
    print(350, WINDOW_HEIGHT - 30, (char *)"SETUP: Click objects, press R to start.");
  }

  if (invincible)
  {
    glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
    print(850, WINDOW_HEIGHT - 60, (char *)"VIP!");
  }
  if (speedBoost)
  {
    glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
    print(850, WINDOW_HEIGHT - 30, (char *)"FAST!");
  }

  glBegin(GL_QUADS);
  glColor3f(0.2f, 0.2f, 0.25f);
  glVertex2f(0, 0);
  glVertex2f(WINDOW_WIDTH, 0);
  glColor3f(0.3f, 0.3f, 0.35f);
  glVertex2f(WINDOW_WIDTH, BOTTOM_PANEL_HEIGHT);
  glVertex2f(0, BOTTOM_PANEL_HEIGHT);
  glEnd();

  drawGuard(100, 50);
  drawBoardingPass(250, 50, 0);
  drawManagerBadge(400, 50, 1.0f);
  drawFastTrackPass(550, 50, 1.0f);

  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  print(70, 20, (char *)"Guard");
  print(205, 20, (char *)"Boarding Pass");
  print(365, 20, (char *)"VIP Badge");
  print(515, 20, (char *)"Fast Track");

  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  print(700, 50, (char *)"Click Below to Select Item.");
  print(700, 20, (char *)"Click on Map to Place.");

  // FIXED: WIN SCREEN with green-to-black gradient banner
  if (gameState == WIN)
  {
    float bannerLeft = 200;
    float bannerRight = 800;
    float bannerBottom = 220;
    float bannerTop = 380;
    
    // Gradient banner (green to black)
    glBegin(GL_QUADS);
    glColor3f(0.0f, 0.6f, 0.0f); // Dark green
    glVertex2f(bannerLeft, bannerBottom);
    glVertex2f(bannerRight, bannerBottom);
    glColor3f(0.0f, 0.0f, 0.0f); // Black
    glVertex2f(bannerRight, bannerTop);
    glVertex2f(bannerLeft, bannerTop);
    glEnd();

    // Border
    glColor3f(0.0f, 1.0f, 0.0f);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    glVertex2f(bannerLeft, bannerBottom);
    glVertex2f(bannerRight, bannerBottom);
    glVertex2f(bannerRight, bannerTop);
    glVertex2f(bannerLeft, bannerTop);
    glEnd();

    // Centered text
    glColor3f(1.0f, 1.0f, 1.0f);
    print(360, 320, (char *)"BOARDING COMPLETE!");
    char winText[100];
    sprintf(winText, "Final Score: %d", score);
    print(430, 280, winText);
    print(270, 240, (char *)"You both caught your flight to Munich (MUC)!");
  }
  // FIXED: LOSE SCREEN with red-to-black gradient banner
  else if (gameState == LOSE)
  {
    float bannerLeft = 200;
    float bannerRight = 800;
    float bannerBottom = 220;
    float bannerTop = 380;
    
    // Gradient banner (red to black)
    glBegin(GL_QUADS);
    glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B); // Romania red
    glVertex2f(bannerLeft, bannerBottom);
    glVertex2f(bannerRight, bannerBottom);
    glColor3f(0.0f, 0.0f, 0.0f); // Black
    glVertex2f(bannerRight, bannerTop);
    glVertex2f(bannerLeft, bannerTop);
    glEnd();

    // Border
    glColor3f(1.0f, 0.0f, 0.0f);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    glVertex2f(bannerLeft, bannerBottom);
    glVertex2f(bannerRight, bannerBottom);
    glVertex2f(bannerRight, bannerTop);
    glVertex2f(bannerLeft, bannerTop);
    glEnd();

    // Centered text
    glColor3f(1.0f, 1.0f, 1.0f);
    print(410, 320, (char *)"FLIGHT MISSED!");
    char loseText[100];
    sprintf(loseText, "Final Score: %d", score);
    print(432, 280, loseText);
    print(227, 240, (char *)"Better luck with booking your next flight... ERRRR x_x");
  }

  glFlush();
}

void timer(int value)
{
  if (gameState == RUNNING)
  {
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

    bezierT += bezierSpeed;
    if (bezierT > 1.0f)
      bezierT = 0.0f;

    int *planePos = bezier(bezierT, bezierP0, bezierP1, bezierP2, bezierP3);
    planeX = planePos[0];
    planeY = planePos[1];

    collectibleRotation += 2.0f;
    if (collectibleRotation >= 360.0f)
      collectibleRotation = 0.0f;

    conveyorOffset += 1.0f;
    if (conveyorOffset >= WINDOW_WIDTH + 50)
      conveyorOffset = 0;

    for (auto &collectible : collectibles)
    {
      collectible.rotation = collectibleRotation;
    }

    for (auto &powerup : powerups)
    {
      powerup.animScale = 0.8f + 0.4f * sin(glutGet(GLUT_ELAPSED_TIME) * 0.01f);
    }

    handleCollisions();

    if (lives <= 0)
    {
      gameState = LOSE;
    }
  }

  glutPostRedisplay();
  glutTimerFunc(16, timer, 0);
}

void keyboard(unsigned char key, int x, int y)
{
  if (gameState == SETUP)
  {
    if (key == 'r' || key == 'R')
    {
      gameState = RUNNING;
    }
    return;
  }

  if (gameState != RUNNING)
    return;

  float moveX = 0, moveY = 0;

  switch (key)
  {
  case 'w':
  case 'W':
    moveY = currentSpeed;
    playerAngle = 90;
    break;
  case 's':
  case 'S':
    moveY = -currentSpeed;
    playerAngle = 270;
    break;
  case 'a':
  case 'A':
    moveX = -currentSpeed;
    playerAngle = 180;
    break;
  case 'd':
  case 'D':
    moveX = currentSpeed;
    playerAngle = 0;
    break;
  }

  cameraOffsetX -= moveX;
  cameraOffsetY -= moveY;

  playerX += moveX;
  playerY += moveY;

  float mapLeft = 50.0f;
  float mapRight = 950.0f;
  float mapTop = 480.0f;
  float mapBottom = 30.0f;
  
  if (playerX < mapLeft) {
    playerX = mapLeft;
    cameraOffsetX += moveX;
  }
  if (playerX > mapRight) {
    playerX = mapRight;
    cameraOffsetX += moveX;
  }
  if (playerY < mapBottom) {
    playerY = mapBottom;
    cameraOffsetY += moveY;
  }
  if (playerY > mapTop) {
    playerY = mapTop;
    cameraOffsetY += moveY;
  }
}

void specialKeys(int key, int x, int y)
{
  if (gameState != RUNNING)
    return;

  float moveX = 0, moveY = 0;

  switch (key)
  {
  case GLUT_KEY_UP:
    moveY = currentSpeed;
    playerAngle = 90;
    break;
  case GLUT_KEY_DOWN:
    moveY = -currentSpeed;
    playerAngle = 270;
    break;
  case GLUT_KEY_LEFT:
    moveX = -currentSpeed;
    playerAngle = 180;
    break;
  case GLUT_KEY_RIGHT:
    moveX = currentSpeed;
    playerAngle = 0;
    break;
  }

  cameraOffsetX -= moveX;
  cameraOffsetY -= moveY;

  playerX += moveX;
  playerY += moveY;

  float mapLeft = 50.0f;
  float mapRight = 950.0f;
  float mapTop = 480.0f;
  float mapBottom = 30.0f;
  
  if (playerX < mapLeft) {
    playerX = mapLeft;
    cameraOffsetX += moveX;
  }
  if (playerX > mapRight) {
    playerX = mapRight;
    cameraOffsetX += moveX;
  }
  if (playerY < mapBottom) {
    playerY = mapBottom;
    cameraOffsetY += moveY;
  }
  if (playerY > mapTop) {
    playerY = mapTop;
    cameraOffsetY += moveY;
  }
}

void mouse(int button, int state, int x, int y)
{
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
  {
    y = WINDOW_HEIGHT - y;

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

    if (y >= GAME_AREA_BOTTOM && y <= GAME_AREA_TOP && drawingMode != NONE)
    {
      float mapX = x - cameraOffsetX;
      float mapY = y - cameraOffsetY;

      bool canPlace = true;

      for (const auto &obstacle : obstacles)
      {
        if (obstacle.active && checkCollision(mapX - 10, mapY - 10, 20, 20,
                                              obstacle.x - obstacle.width / 2, obstacle.y - obstacle.height / 2,
                                              obstacle.width, obstacle.height))
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
          obstacles.push_back({mapX, mapY, 16, 24, true, 0, 0});
          break;
        case COLLECTIBLE:
          collectibles.push_back({mapX, mapY, 16, 10, true, 0, 0});
          break;
        case POWERUP1:
          powerups.push_back({mapX, mapY, true, 1.0f, 1});
          break;
        case POWERUP2:
          powerups.push_back({mapX, mapY, true, 1.0f, 2});
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