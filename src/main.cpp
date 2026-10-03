#include "ST7305_U8g2.h"

#define CLICKSETTIME 200000
#define CLICKCLEARTIME 200000

#define INITSCALE (int32_t)(300 * PI / 2)
#define INITSPEED 0.0008f
#define SCALESPEED 1.005f
#define MOVESPEED (1<<15)
#define ANIMSPEEDCHANGE 0.00001f

#define CURVECOUNT 256
#define CURVESTEP 4
#define ITERATIONS 256
#define SCREENWIDTH 400
#define SCREENHEIGHT 304
#define PI 3.1415926535897932384626433832795
#define SINTABLEPOWER 16
#define SINTABLEENTRIES (1 << SINTABLEPOWER)
#define ANG1INC (int32_t)((CURVESTEP * SINTABLEENTRIES) / 235)
#define ANG2INC (int32_t)((CURVESTEP * SINTABLEENTRIES) / (2 * PI))

#define LEFT_BUTTON_PIN 18
#define RIGHT_BUTTON_PIN 0

#define I2C_SDA_PIN 13
#define I2C_SCL_PIN 14

#define RLCD_SCK_PIN 11
#define RLCD_MOSI_PIN 12
#define RLCD_DC_PIN 5
#define RLCD_CS_PIN 40
#define RLCD_RST_PIN 41

ST7305_U8g2 lcd(RLCD_SCK_PIN, RLCD_MOSI_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN);
U8G2* u8g2;
uint8_t* buffer;
uint8_t* screenCentre;

bool oldLeftButton = false;
bool oldRightButton = false;
int leftButtonClicks = 0;
int leftClickSetTime = 0;
int leftClickClearTime = 0;
int rightButtonClicks = 0;
int rightClickSetTime = 0;
int rightClickClearTime = 0;

int32_t xOffset = 0;
int32_t yOffset = ((304-300)/2) << SINTABLEPOWER;
float speed = INITSPEED;
float scale = INITSCALE;
float animationTime = 0.0f;
uint32_t oldTime = 0;
int32_t sinTable[SINTABLEENTRIES];

void makeSinTable()
{
    for (int i = 0; i < SINTABLEENTRIES; ++i)
    {
        sinTable[i] = sin(i * 2 * PI / SINTABLEENTRIES) * SINTABLEENTRIES / (2 * PI);
    }
    for (int i = 0; i < SINTABLEENTRIES; ++i)
    {
        *((int16_t *)(sinTable + i) + 1) = *(int16_t *)(sinTable + ((i + SINTABLEENTRIES / 4) % SINTABLEENTRIES));
    }
}

void setup()
{
    Serial.begin(115200);

    lcd.begin(0, U8G2_R1);
    u8g2 = lcd.getU8g2();
    buffer = u8g2->getBufferPtr();
    memset(buffer, 0, (SCREENWIDTH>>3) * SCREENHEIGHT);
    u8g2->sendBuffer();
    screenCentre = buffer + ((SCREENHEIGHT + (SCREENWIDTH>>3) * SCREENHEIGHT) >> 1);;

    u8g2->setFont(u8g2_font_6x13_tf);
    u8g2->setDrawColor(1);

    pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);

    makeSinTable();
    oldTime = micros();
}

void controls(uint32_t deltaTime)
{
    bool leftButton = digitalRead(LEFT_BUTTON_PIN) == LOW;
    bool rightButton = digitalRead(RIGHT_BUTTON_PIN) == LOW;
    bool anyButton = leftButton || rightButton;

    if (leftButton && !oldLeftButton) { leftClickClearTime = 0; leftClickSetTime = CLICKSETTIME; }
    if (!leftButton && oldLeftButton) { leftButtonClicks = leftClickSetTime > 0 ? leftButtonClicks + 1 : 0; leftClickSetTime = 0; leftClickClearTime = CLICKCLEARTIME; }
    if (!anyButton && leftClickClearTime <= 0) { leftButtonClicks = 0; }
    if (rightButton && !oldRightButton) { rightClickClearTime = 0; rightClickSetTime = CLICKSETTIME; }
    if (!rightButton && oldRightButton) { rightButtonClicks = rightClickSetTime > 0 ? rightButtonClicks + 1 : 0; rightClickSetTime = 0; rightClickClearTime = CLICKCLEARTIME; }
    if (!anyButton && rightClickClearTime <= 0) { rightButtonClicks = 0; }

    if (leftClickSetTime > 0) { leftClickSetTime -= deltaTime; }
    if (leftClickClearTime > 0) { leftClickClearTime -= deltaTime; }
    if (rightClickSetTime > 0) { rightClickSetTime -= deltaTime; }
    if (rightClickClearTime > 0) { rightClickClearTime -= deltaTime; }

    if (leftButton && leftClickSetTime <= 0)
    {
        if (leftButtonClicks == 0 && rightButtonClicks == 0) { scale /= SCALESPEED; xOffset /= SCALESPEED; yOffset /= SCALESPEED; }
        if (leftButtonClicks == 1) { yOffset += MOVESPEED; }
        if (rightButtonClicks == 1) { xOffset += MOVESPEED; }
        if (leftButtonClicks == 2) { speed += ANIMSPEEDCHANGE; }
    }
    if (rightButton && rightClickSetTime <= 0)
    {
        if (rightButtonClicks == 0 && leftButtonClicks == 0) { scale *= SCALESPEED; xOffset *= SCALESPEED; yOffset *= SCALESPEED; }
        if (rightButtonClicks == 1) { yOffset -= MOVESPEED; }
        if (leftButtonClicks == 1) { xOffset -= MOVESPEED; }
        if (rightButtonClicks == 2) { speed -= ANIMSPEEDCHANGE; }
    }

    oldLeftButton = leftButton;
    oldRightButton = rightButton;

    Serial.printf("%d %d %d %d %d %d\n", leftButtonClicks, leftClickSetTime, leftClickClearTime, rightButtonClicks, rightClickSetTime, rightClickClearTime);
}

void loop()
{
    uint32_t newTime = micros();
    uint32_t deltaTime = newTime - oldTime;
    oldTime = newTime;
    animationTime += speed * deltaTime;

    u8g2->clearBuffer();

    int32_t ang1Start = animationTime;
    int32_t ang2Start = animationTime;
    int32_t scaleMul = scale;
    for (int i = 0; i < CURVECOUNT; i += CURVESTEP)
    {
        int32_t x = 0;
        int32_t y = 0;
        for (int j = 0; j < ITERATIONS; ++j)
        {
            int32_t values1 = sinTable[(ang1Start + x) & (SINTABLEENTRIES - 1)];
            int32_t values2 = sinTable[(ang2Start + y) & (SINTABLEENTRIES - 1)];
            x = (int32_t)(int16_t)values1 + (int32_t)(int16_t)values2;
            y = (values1 >> 16) + (values2 >> 16);
            int32_t pX = (x * scaleMul + xOffset) >> SINTABLEPOWER;
            int32_t pY = -(y * scaleMul + yOffset) >> SINTABLEPOWER;
            if (pX >= -SCREENWIDTH / 2 && pX < SCREENWIDTH / 2 && pY >= -SCREENHEIGHT / 2 && pY < SCREENHEIGHT / 2)
            {
                uint8_t* addr = screenCentre + (pX>>3) * SCREENHEIGHT + pY;
                *addr |= 1 << (pX & 7);
            }
        }
        ang1Start += ANG1INC;
        ang2Start += ANG2INC;
    }

    controls(deltaTime);

    u8g2->sendBuffer();

    Serial.printf("%lums %lu\n", (unsigned long)(deltaTime/1000), (unsigned long)(speed * deltaTime));
}
