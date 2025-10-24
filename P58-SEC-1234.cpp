#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
// NEW: Define GL_SILENCE_DEPRECATION before including OpenGL headers to suppress warnings
#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <unistd.h> 

// ROMANIA FLAG COLORS (Values kept, but included for context)
#define ROMANIA_BLUE_R 0.0f
#define ROMANIA_BLUE_G 0.169f
#define ROMANIA_BLUE_B 0.498f // #002B7F

#define ROMANIA_YELLOW_R 0.988f
#define ROMANIA_YELLOW_G 0.820f
#define ROMANIA_YELLOW_B 0.086f // #FCD116

#define ROMANIA_RED_R 0.808f
#define ROMANIA_RED_G 0.067f
#define ROMANIA_RED_B 0.149f // #CE1126

// --- Existing BMP Texture Loading ---

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

  // Allocate memory for image data
  int dataSize = width * height * 3;
  unsigned char *imageData = (unsigned char *)malloc(dataSize);
  if (!imageData) {
    printf("ERROR: Could not allocate memory for image data.\n");
    fclose(file);
    return 0;
  }

  // Seek to image data
  fseek(file, dataOffset, SEEK_SET);
  fread(imageData, 1, dataSize, file);
  fclose(file);

  // Flip image vertically (BMP stores bottom-up)
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

  // Check for OpenGL errors
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

// --- Add camera variables ---
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

// Player
float playerX = 500, playerY = 50; // Start position at the very bottom of the map
float playerAngle = 0;
const float PLAYER_SIZE = 20;
const float PLAYER_SPEED = 3.0f;
float currentSpeed = PLAYER_SPEED;

// Game Target (Plane on Runway)
// Position the plane at the actual runway coordinates on the airport map
float planeX = 500, planeY = 450; // Positioned on the runway of the airport map
float bezierT = 0.0f;
float bezierSpeed = 0.01f;
// Bezier curve points for plane movement along the runway (relative to map coordinates)
int bezierP0[2] = {400, 450};  // Start of runway
int bezierP1[2] = {500, 450};  // Middle of runway
int bezierP2[2] = {600, 450};  // End of runway
int bezierP3[2] = {500, 450};  // Back to middle

// Game Objects
std::vector<GameObject> obstacles;
std::vector<GameObject> collectibles;
std::vector<PowerUp> powerups;
GameObject friendObj = {487, 400, 30, 35, true, 0, 0}; // Friend positioned at top of map
bool friendCollected = false;

// Game Stats
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

// Power-up Effects
bool invincible = false;
float invincibleTimer = 0;
bool speedBoost = false;
float speedBoostTimer = 0;

// Animation Variables
float collectibleRotation = 0;
float conveyorOffset = 0;

// Texture IDs
GLuint mapTexture;
GLuint playerTexture, planeTexture, guardTexture, boardingPassTexture;
GLuint friendTexture, badgeTexture, fastTrackTexture, luggageTexture, panelTexture;

// FORWARD DECLARATION FOR COLLISION FUNCTION (Fixes the 'undeclared identifier' error)
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

// ------------------------------------------------------------------
// --- DRAWING FUNCTIONS (Kept from Phase 1) ---
// ------------------------------------------------------------------

void drawPlayer(float x, float y, float angle)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glRotatef(angle, 0, 0, 1);

  // Head/Skin tone
  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(5 * cosf(theta), 8 + 5 * sinf(theta));
  }
  glEnd();

  // Hair/Beanie
  glColor3f(0.1f, 0.1f, 0.15f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(6 * cosf(theta), 9 + 6 * sinf(theta));
  }
  glEnd();

  // Jacket
  glColor3f(0.4f, 0.4f, 0.5f);
  glBegin(GL_POLYGON);
  glVertex2f(-8, 3);
  glVertex2f(8, 3);
  glVertex2f(10, -8);
  glVertex2f(-10, -8);
  glEnd();

  // Jacket Zipper/Stripe (Romania Blue Accent)
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glLineWidth(2);
  glBegin(GL_LINES);
  glVertex2f(0, 3);
  glVertex2f(0, -5);
  glEnd();

  // Trousers
  glColor3f(0.2f, 0.2f, 0.25f);
  glBegin(GL_QUADS);
  glVertex2f(-10, -8);
  glVertex2f(10, -8);
  glVertex2f(8, -15);
  glVertex2f(-8, -15);
  glEnd();

  // Luggage
  glColor3f(0.7f, 0.5f, 0.3f);
  glBegin(GL_QUADS);
  glVertex2f(10, 0);
  glVertex2f(16, 0);
  glVertex2f(16, -10);
  glVertex2f(10, -10);
  glEnd();

  // Luggage tag (Romania Red)
  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_QUADS);
  glVertex2f(13, -8);
  glVertex2f(15, -8);
  glVertex2f(15, -6);
  glVertex2f(13, -6);
  glEnd();

  // Luggage handle (Romania Yellow)
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glLineWidth(3);
  glBegin(GL_LINE_STRIP);
  glVertex2f(13, 0);
  glVertex2f(13, 3);
  glEnd();

  // Arms
  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_TRIANGLES);
  glVertex2f(-8, 2);
  glVertex2f(-12, 0);
  glVertex2f(-8, -2);
  glEnd();

  // Eyes
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

  // Fuselage - white/silver
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

  // Wings - light gray
  glColor3f(0.85f, 0.85f, 0.88f);
  glBegin(GL_TRIANGLES);
  // Left wing
  glVertex2f(-20, 0);
  glVertex2f(-35, -18);
  glVertex2f(-10, -18);
  // Right wing
  glVertex2f(20, 0);
  glVertex2f(35, -18);
  glVertex2f(10, -18);
  glEnd();

  // Tail with Romania flag colors
  float tailWidth = 10;
  float tailHeight = 12;

  // Blue
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_QUADS);
  glVertex2f(-30, 0);
  glVertex2f(-30 - tailWidth / 3, 0);
  glVertex2f(-30 - tailWidth / 3, tailHeight);
  glVertex2f(-30, tailHeight);
  glEnd();

  // Yellow
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_QUADS);
  glVertex2f(-30 - tailWidth / 3, 0);
  glVertex2f(-30 - 2 * tailWidth / 3, 0);
  glVertex2f(-30 - 2 * tailWidth / 3, tailHeight);
  glVertex2f(-30 - tailWidth / 3, tailHeight);
  glEnd();

  // Red
  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_QUADS);
  glVertex2f(-30 - 2 * tailWidth / 3, 0);
  glVertex2f(-30 - tailWidth, 0);
  glVertex2f(-30 - tailWidth, tailHeight);
  glVertex2f(-30 - 2 * tailWidth / 3, tailHeight);
  glEnd();

  // Windows
  glColor3f(0.4f, 0.6f, 0.8f);
  glPointSize(4);
  glBegin(GL_POINTS);
  for (int i = -20; i < 20; i += 8)
  {
    glVertex2f(i, 2);
  }
  glEnd();

  // "GATE A01" text
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

  // Head/Skin tone
  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(4 * cosf(theta), 10 + 4 * sinf(theta));
  }
  glEnd();

  // Security cap with Romania blue
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_POLYGON);
  for (int i = 10; i < 30; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(5 * cosf(theta), 11 + 4.5f * sinf(theta));
  }
  glEnd();

  // Cap visor (darker shade)
  glColor3f(0.1f, 0.1f, 0.2f);
  glBegin(GL_QUADS);
  glVertex2f(-6, 10);
  glVertex2f(6, 10);
  glVertex2f(7, 8);
  glVertex2f(-7, 8);
  glEnd();

  // Uniform - dark navy blue/gray
  glColor3f(0.1f, 0.1f, 0.3f);
  glBegin(GL_QUADS);
  glVertex2f(-7, 7);
  glVertex2f(7, 7);
  glVertex2f(7, -12);
  glVertex2f(-7, -12);
  glEnd();

  // Tie (Romania Red Accent)
  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_TRIANGLES);
  glVertex2f(-1, 5);
  glVertex2f(1, 5);
  glVertex2f(0, 0);
  glEnd();

  // Badge with Romania yellow (Security Star)
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 6; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 6.0f;
    glVertex2f(-4 + 2 * cosf(theta), 2 + 2 * sinf(theta));
  }
  glEnd();

  // Arms
  glColor3f(0.95f, 0.87f, 0.73f);
  glBegin(GL_QUADS);
  glVertex2f(-7, 5);
  glVertex2f(-10, 3);
  glVertex2f(-10, -3);
  glVertex2f(-7, -1);
  glEnd();

  // Flashlight/baton
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

  // Paper background
  glColor3f(1.0f, 0.98f, 0.95f);
  glBegin(GL_QUADS);
  glVertex2f(-10, -6);
  glVertex2f(10, -6);
  glVertex2f(10, 6);
  glVertex2f(-10, 6);
  glEnd();

  // Tear-off section perforation
  glColor3f(0.7f, 0.7f, 0.7f);
  glLineWidth(1);
  glLineStipple(1, 0xAAAA);
  glEnable(GL_LINE_STIPPLE);
  glBegin(GL_LINES);
  glVertex2f(2, -6);
  glVertex2f(2, 6);
  glEnd();
  glDisable(GL_LINE_STIPPLE);

  // Romania flag stripe - top left
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

  // "CLJ" / "OTP" airport code (realistic text)
  glColor3f(0.0f, 0.0f, 0.0f);
  glRasterPos2f(-8, 3);
  char airportCode[] = "CLJ-OTP";
  for (int i = 0; i < 7; i++)
  {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, airportCode[i]);
  }

  // Barcode
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

  // Head/Skin tone (smaller for petite build)
  glColor3f(0.92f, 0.84f, 0.70f);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(5 * cosf(theta), 10 + 5 * sinf(theta));
  }
  glEnd();

  // Headscarf - Modern hijab style (covering head and shoulders)
  glColor3f(1.0f, 1.0f, 1.0f); // White hijab
  glBegin(GL_POLYGON);
  // Main scarf covering head
  glVertex2f(-8, 12);
  glVertex2f(8, 12);
  glVertex2f(10, 2);
  glVertex2f(8, -2);
  glVertex2f(-8, -2);
  glVertex2f(-10, 2);
  glEnd();

  // Scarf drape on shoulder
  glColor3f(0.9f, 0.9f, 0.9f); // Slightly off-white for depth
  glBegin(GL_POLYGON);
  glVertex2f(-8, -2);
  glVertex2f(8, -2);
  glVertex2f(6, -8);
  glVertex2f(-6, -8);
  glEnd();

  // Body with modern outfit (smaller for petite build)
  glColor3f(0.3f, 0.4f, 0.6f); // Modern blue top
  glBegin(GL_POLYGON);
  glVertex2f(-8, 2);
  glVertex2f(8, 2);
  glVertex2f(9, -8);
  glVertex2f(-9, -8);
  glEnd();

  // Pants (modern fit)
  glColor3f(0.2f, 0.2f, 0.3f);
  glBegin(GL_QUADS);
  glVertex2f(-9, -8);
  glVertex2f(9, -8);
  glVertex2f(8, -15);
  glVertex2f(-8, -15);
  glEnd();

  // Small backpack
  glColor3f(0.4f, 0.4f, 0.4f);
  glBegin(GL_QUADS);
  glVertex2f(-12, 0);
  glVertex2f(-8, 0);
  glVertex2f(-8, -6);
  glVertex2f(-12, -6);
  glEnd();

  // Backpack straps
  glColor3f(0.2f, 0.2f, 0.2f);
  glLineWidth(2);
  glBegin(GL_LINES);
  glVertex2f(-10, 0);
  glVertex2f(-6, 2);
  glVertex2f(-10, -3);
  glVertex2f(-6, -1);
  glEnd();

  // Arms (smaller for petite build)
  glColor3f(0.92f, 0.84f, 0.70f);
  glBegin(GL_TRIANGLES);
  glVertex2f(8, 1);
  glVertex2f(12, 4);
  glVertex2f(8, -2);
  glEnd();

  // Face area - White circle (niqab style)
  glColor3f(1.0f, 1.0f, 1.0f); // Pure white
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(4 * cosf(theta), 8 + 4 * sinf(theta));
  }
  glEnd();

  // Very light skin tone - smaller circle inside
  glColor3f(0.98f, 0.95f, 0.90f); // Very light skin tone
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(2.5f * cosf(theta), 8 + 2.5f * sinf(theta));
  }
  glEnd();

  // Eyes (small dots)
  glColor3f(0.0f, 0.0f, 0.0f);
  glPointSize(1);
  glBegin(GL_POINTS);
  glVertex2f(-1, 9);
  glVertex2f(1, 9);
  glEnd();

  glPopMatrix();
}

void drawManagerBadge(float x, float y, float scale)
{
  glPushMatrix();
  glTranslatef(x, y, 0);
  glScalef(scale, scale, 1);

  // Badge shape - Romania yellow
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 6; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 6.0f;
    glVertex2f(10 * cosf(theta), 10 * sinf(theta));
  }
  glEnd();

  // Inner circle with Romania Blue
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_POLYGON);
  for (int i = 0; i < 20; i++)
  {
    float theta = 2.0f * 3.1415926f * float(i) / 20.0f;
    glVertex2f(7 * cosf(theta), 7 * sinf(theta));
  }
  glEnd();

  // "VIP" text
  glColor3f(1.0f, 1.0f, 1.0f);
  glRasterPos2f(-6, -2);
  char vipText[] = "VIP";
  for (int i = 0; i < 3; i++)
  {
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, vipText[i]);
  }

  // Outer border - Black
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

  // Card - Romania colors: Blue, Yellow, Red stripes
  float stripWidth = 6.6f;

  // Red Stripe
  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glBegin(GL_QUADS);
  glVertex2f(-10, -6);
  glVertex2f(-10 + stripWidth, -6);
  glVertex2f(-10 + stripWidth, 6);
  glVertex2f(-10, 6);
  glEnd();

  // Yellow Stripe
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glBegin(GL_QUADS);
  glVertex2f(-10 + stripWidth, -6);
  glVertex2f(-10 + 2 * stripWidth, -6);
  glVertex2f(-10 + 2 * stripWidth, 6);
  glVertex2f(-10 + stripWidth, 6);
  glEnd();

  // Blue Stripe
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glBegin(GL_QUADS);
  glVertex2f(-10 + 2 * stripWidth, -6);
  glVertex2f(10, -6);
  glVertex2f(10, 6);
  glVertex2f(-10 + 2 * stripWidth, 6);
  glEnd();

  // Fast forward arrows in White
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

  // Luggage outline
  glColor3f(0.4f, 0.3f, 0.2f);
  glBegin(GL_QUADS);
  glVertex2f(-10, -7);
  glVertex2f(10, -7);
  glVertex2f(10, 7);
  glVertex2f(-10, 7);
  glEnd();

  // Fill indicator (Green/Red health bar effect)
  if (filled)
  {
    glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B); // Red when alive
  }
  else
  {
    glColor3f(0.3f, 0.3f, 0.3f); // Gray when lost
  }
  glBegin(GL_QUADS);
  glVertex2f(-8, -5);
  glVertex2f(8, -5);
  glVertex2f(8, 5);
  glVertex2f(-8, 5);
  glEnd();

  // Handle
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

  // Simple approach: Draw the full map texture with camera offset
  // The camera transformation is already applied in the display function
  glColor3f(1.0f, 1.0f, 1.0f);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, mapTexture);
  
  // Draw the full map texture covering the entire game area
  // The camera offset will move this texture as needed
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f); glVertex2f(0, GAME_AREA_BOTTOM);
  glTexCoord2f(1.0f, 1.0f); glVertex2f(WINDOW_WIDTH, GAME_AREA_BOTTOM);
  glTexCoord2f(1.0f, 0.0f); glVertex2f(WINDOW_WIDTH, GAME_AREA_TOP);
  glTexCoord2f(0.0f, 0.0f); glVertex2f(0, GAME_AREA_TOP);
  glEnd();
  
  glDisable(GL_TEXTURE_2D);
}

// ------------------------------------------------------------------
// --- Collision Function Definition (Fixes the 'undeclared identifier' error) ---
// ------------------------------------------------------------------
bool checkCollision(float x1, float y1, float w1, float h1,
                    float x2, float y2, float w2, float h2)
{
  return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}
// ------------------------------------------------------------------

void handleCollisions()
{
  // Debug: Print player position and object positions
  static int debugCounter = 0;
  if (debugCounter % 60 == 0) { // Print every second
    printf("DEBUG: Player at (%.1f, %.1f), Friend at (%.1f, %.1f), Collectibles: %zu, Lives: %d, Score: %d\n", 
           playerX, playerY, friendObj.x, friendObj.y, collectibles.size(), lives, score);
  }
  debugCounter++;

  // Check collision with movable obstacles (Guards)
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
        // Simple push-back collision response
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

  // Collectible collision logic
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

  // Friend collision
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

  // Powerup collision
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

  // Check collision with plane on runway
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
  // --- Load the Map Texture ---
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

  // Reset camera
  cameraOffsetX = 0;
  cameraOffsetY = 0;
  
  // Set player starting position (bottom of map)
  playerX = 500;
  playerY = 50;
  playerAngle = 0;
  gameState = SETUP;
  score = 0;
  lives = 5;
  gameTime = 60;
  gameTimer = 0;
  friendCollected = false;

  // Set plane position at top of map (runway area)
  planeX = 500;
  planeY = 450;  // This is now a MAP coordinate, not screen coordinate
  
  // Set friend position at the top of the map near the plane
  friendObj = {487, 400, 30, 35, true, 0, 0};
  
  // Set camera to show player at bottom of map initially
  // Player is at (500, 50) on map, but we want to show them at screen center (500, 300)
  // So camera should be offset to show the bottom of the map
  cameraOffsetX = 0;
  cameraOffsetY = 250;

  obstacles.clear();
  collectibles.clear();
  powerups.clear();
}

// --- Display Function ---

void display()
{
  glClear(GL_COLOR_BUFFER_BIT);

  // 1. Draw Map Background with camera offset
  glPushMatrix();
  glTranslatef(cameraOffsetX, cameraOffsetY, 0);
  drawMapBackground();

  // 2. Draw Game Objects with camera offset
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

  // Draw the plane at its fixed map position (not screen position)
  drawPlane(planeX, planeY);
  
  // Draw player at center of screen (no camera offset for player)
  glPopMatrix(); // End camera transformation
  drawPlayer(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, playerAngle);

  // 3. TOP PANEL - UI (Remains the same)
  glColor3f(0.1f, 0.1f, 0.15f);
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
  glVertex2f(0, GAME_AREA_TOP);
  glVertex2f(WINDOW_WIDTH, GAME_AREA_TOP);
  glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
  glVertex2f(0, WINDOW_HEIGHT);
  glEnd();

  // Romania Flag Accent Bar at the top edge
  float flagBarHeight = 10;
  glBegin(GL_QUADS);
  // Blue
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  glVertex2f(0, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  glVertex2f(0, WINDOW_HEIGHT);
  // Yellow
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  glVertex2f(WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  // Red
  glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT - flagBarHeight);
  glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
  glVertex2f(2 * WINDOW_WIDTH / 3, WINDOW_HEIGHT);
  glEnd();

  // Draw health indicators
  for (int i = 0; i < 5; i++)
  {
    drawStressIndicator(50 + i * 40, WINDOW_HEIGHT - 50, i < lives);
  }

  // Draw score and time with high visibility
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  char scoreText[50];
  sprintf(scoreText, "SCORE: %d", score);
  print(250, WINDOW_HEIGHT - 60, scoreText);

  char timeText[50];
  sprintf(timeText, "TIME: %d sec", gameTime);
  print(450, WINDOW_HEIGHT - 60, timeText);

  // Friend status
  if (friendCollected)
  {
    glColor3f(0.0f, 1.0f, 0.0f); // Green for OK
    print(650, WINDOW_HEIGHT - 60, (char *)"FRIEND: OK!");
  }
  else
  {
    glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B); // Red for Missing
    print(650, WINDOW_HEIGHT - 60, (char *)"FIND FRIEND!");
  }

  if (gameState == SETUP)
  {
    glColor3f(ROMANIA_RED_R, ROMANIA_RED_G, ROMANIA_RED_B);
    print(350, WINDOW_HEIGHT - 30, (char *)"SETUP: Click objects, press R to start.");
  }

  // Power-up status
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

  // 4. BOTTOM PANEL - UI (Remains the same)
  glBegin(GL_QUADS);
  glColor3f(0.2f, 0.2f, 0.25f);
  glVertex2f(0, 0);
  glVertex2f(WINDOW_WIDTH, 0);
  glColor3f(0.3f, 0.3f, 0.35f);
  glVertex2f(WINDOW_WIDTH, BOTTOM_PANEL_HEIGHT);
  glVertex2f(0, BOTTOM_PANEL_HEIGHT);
  glEnd();

  // Draw template objects
  drawGuard(100, 50);
  drawBoardingPass(250, 50, 0);
  drawManagerBadge(400, 50, 1.0f);
  drawFastTrackPass(550, 50, 1.0f);

  // Labels with Romania colors
  glColor3f(ROMANIA_YELLOW_R, ROMANIA_YELLOW_G, ROMANIA_YELLOW_B);
  print(70, 20, (char *)"Guard");
  print(205, 20, (char *)"Boarding Pass");
  print(365, 20, (char *)"VIP Badge");
  print(515, 20, (char *)"Fast Track");

  // Instruction
  glColor3f(ROMANIA_BLUE_R, ROMANIA_BLUE_G, ROMANIA_BLUE_B);
  print(700, 50, (char *)"Click Below to Select Item.");
  print(700, 20, (char *)"Click on Map to Place.");

  // Game end screens with centered text and background
  if (gameState == WIN)
  {
    // Dark background behind text for better readability
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(180, 240);
    glVertex2f(820, 240);
    glVertex2f(820, 360);
    glVertex2f(180, 360);
    glEnd();

    // Centered text
    glColor3f(1.0f, 1.0f, 1.0f);
    char winText[100];
    sprintf(winText, "BOARDING COMPLETE! Score: %d", score);
    print(350, 300, winText);
    print(380, 270, (char *)"You caught your flight to Munich!");
  }
  else if (gameState == LOSE)
  {
    // Dark background behind text for better readability
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(180, 240);
    glVertex2f(820, 240);
    glVertex2f(820, 360);
    glVertex2f(180, 360);
    glEnd();

    // Centered text
    glColor3f(1.0f, 1.0f, 1.0f);
    char loseText[100];
    sprintf(loseText, "FLIGHT MISSED! Score: %d", score);
    print(400, 300, loseText);
    print(420, 270, (char *)"Better luck next time!");
  }

  glFlush();
}

// --- Input Functions ---

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

    // Move the plane along the runway using Bezier curve from bezier.cpp
    int *planePos = bezier(bezierT, bezierP0, bezierP1, bezierP2, bezierP3);
    planeX = planePos[0];
    planeY = planePos[1]; // Keep plane on the runway level (y=450)

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

  // Move camera in opposite direction to create player-centered movement
  cameraOffsetX -= moveX;
  cameraOffsetY -= moveY;

  // Update player's map position (for collision detection)
  playerX += moveX;
  playerY += moveY;

  // Boundary check - prevent player from going outside map boundaries
  float mapLeft = 50.0f;
  float mapRight = 950.0f;
  float mapTop = 480.0f;  // Allow player to reach the plane area
  float mapBottom = 30.0f;  // Allow player to go to very bottom
  
  if (playerX < mapLeft) {
    playerX = mapLeft;
    cameraOffsetX += moveX; // Cancel camera movement
  }
  if (playerX > mapRight) {
    playerX = mapRight;
    cameraOffsetX += moveX; // Cancel camera movement
  }
  if (playerY < mapBottom) {
    playerY = mapBottom;
    cameraOffsetY += moveY; // Cancel camera movement
  }
  if (playerY > mapTop) {
    playerY = mapTop;
    cameraOffsetY += moveY; // Cancel camera movement
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

  // Move camera in opposite direction to create player-centered movement
  cameraOffsetX -= moveX;
  cameraOffsetY -= moveY;

  // Update player's map position (for collision detection)
  playerX += moveX;
  playerY += moveY;

  // Boundary check - prevent player from going outside map boundaries
  float mapLeft = 50.0f;
  float mapRight = 950.0f;
  float mapTop = 480.0f;  // Allow player to reach the plane area
  float mapBottom = 30.0f;  // Allow player to go to very bottom
  
  if (playerX < mapLeft) {
    playerX = mapLeft;
    cameraOffsetX += moveX; // Cancel camera movement
  }
  if (playerX > mapRight) {
    playerX = mapRight;
    cameraOffsetX += moveX; // Cancel camera movement
  }
  if (playerY < mapBottom) {
    playerY = mapBottom;
    cameraOffsetY += moveY; // Cancel camera movement
  }
  if (playerY > mapTop) {
    playerY = mapTop;
    cameraOffsetY += moveY; // Cancel camera movement
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
      // Convert screen coordinates to map coordinates using camera offset
      float mapX = x - cameraOffsetX;
      float mapY = y - cameraOffsetY;

      bool canPlace = true;

      // Check against obstacles (using map coordinates)
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