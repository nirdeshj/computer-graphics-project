#include <graphics.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

struct Point {
    int x, y;
};

struct Seed {
    double x, y;
    double angle;
    double velocityY;
    bool active;
};

class AnimatedTreeDrawer {
   private:
    int screenWidth, screenHeight;
    int groundLevel;
    int seedX, seedY;
    std::chrono::steady_clock::time_point lastTime;
    double sunAngle;  // for moving sun

    // Animation state variables
    double treeGrowthScale;
    double flowerScale;
    bool showFlowers;
    int flowerPosX, flowerPosY;
    std::vector<Point> flowerPositions;
    std::vector<Seed> fallingSeeds;
    int animationPhase;  // 0: seed germination, 1: tree growth, 2: flowering, 3: seed fall, 4: reset
    int phaseTimer;
    int rightmostBranchX, rightmostBranchY;
    double zoomScale;
    int cameraOffsetX, cameraOffsetY;

    // Colors
    const int BROWN = COLOR(139, 69, 19);
    const int DARK_BROWN = COLOR(101, 67, 33);
    const int LEAF_GREEN = COLOR(34, 139, 34);
    const int LIGHT_GREEN = COLOR(50, 205, 50);
    const int SKY_BLUE = COLOR(135, 206, 235);
    const int SOIL_BROWN = COLOR(90, 50, 20);

    // Draw a seed with rotation
    void drawSeed(int x, int y, double angle, double scale = 1.0) {
        int oldColor = getcolor();

        setcolor(COLOR(160, 82, 45));
        setfillstyle(SOLID_FILL, COLOR(160, 82, 45));

        int size = static_cast<int>(8 * scale);

        // Draw seed as rotated oval using rotation transformation
        // Calculate 4 points of the oval and rotate them
        const int numPoints = 12;
        int points[numPoints * 2];

        for (int i = 0; i < numPoints; i++) {
            double t = i * 2 * 3.14159 / numPoints;
            // Oval shape (wider than tall)
            double localX = size * cos(t);
            double localY = size * 0.5 * sin(t);

            // Apply rotation transformation
            int rotatedX = static_cast<int>(localX * cos(angle) - localY * sin(angle));
            int rotatedY = static_cast<int>(localX * sin(angle) + localY * cos(angle));

            points[i * 2] = x + rotatedX;
            points[i * 2 + 1] = y + rotatedY;
        }

        // Fill the rotated seed
        fillpoly(numPoints, points);

        // Draw a line through the seed to show rotation clearly
        int lineLength = static_cast<int>(size * 0.8);
        int lineX1 = x + static_cast<int>(lineLength * cos(angle));
        int lineY1 = y + static_cast<int>(lineLength * sin(angle));
        int lineX2 = x - static_cast<int>(lineLength * cos(angle));
        int lineY2 = y - static_cast<int>(lineLength * sin(angle));

        setcolor(COLOR(100, 50, 20));
        setlinestyle(SOLID_LINE, 0, std::max(1, static_cast<int>(scale / 3)));
        line(lineX1, lineY1, lineX2, lineY2);

        setcolor(oldColor);
    }

    // Draw soil layers
    void drawSoil() {
        // Ground surface
        setcolor(DARK_BROWN);
        setfillstyle(SOLID_FILL, DARK_BROWN);
        bar(0, groundLevel, screenWidth, groundLevel + 50);

        // Underground soil (darker)
        setcolor(SOIL_BROWN);
        setfillstyle(SOLID_FILL, SOIL_BROWN);
        bar(0, groundLevel + 50, screenWidth, screenHeight);
    }
    // Draw a branch recursively with scaling
    void drawBranch(int x1, int y1, double length, double angle, int depth, double scale, double growthProgress = 1.0) {
        // OPTIMIZATION 1: Cull branches that are too small to see.
        // If the branch is less than 2 pixels long, don't waste CPU drawing it.
        double currentVisualLength = length * scale;
        if (currentVisualLength < 2.0 || depth <= 0) return;

        double branchProgress = std::min(1.0, std::max(0.0, (growthProgress * 10) - (8 - depth)));
        if (branchProgress <= 0) return;

        double scaledLength = currentVisualLength * branchProgress;

        int x2 = x1 + static_cast<int>(scaledLength * cos(angle));
        int y2 = y1 - static_cast<int>(scaledLength * sin(angle));

        // Track rightmost position for flower/seed
        if (depth == 1 && x2 > rightmostBranchX) {
            rightmostBranchX = x2;
            rightmostBranchY = y2;
        }

        // Draw the Branch
        if (depth > 4) {
            setcolor(BROWN);
            setlinestyle(SOLID_LINE, 0, static_cast<int>(depth * scale) + 1);
        } else {
            setcolor(COLOR(101, 67, 33));
            // OPTIMIZATION 2: Simpler lines for small branches
            setlinestyle(SOLID_LINE, 0, std::max(1, static_cast<int>(depth * scale)));
        }

        line(x1, y1, x2, y2);

        // OPTIMIZATION 3: Reduce leaf density
        // Only draw complex leaves if the branch is large enough
        if (depth <= 5 && scale > 0.5 && branchProgress > 0.8 && currentVisualLength > 5.0) {
            setcolor(LIGHT_GREEN);
            setfillstyle(SOLID_FILL, LIGHT_GREEN);

            // Draw fewer leaves at lower depths to save FPS
            int numLeaves = (depth <= 2) ? 5 : 2;

            for (int i = 0; i < numLeaves; i++) {
                int leafX = x2 + (rand() % 12 - 6);
                int leafY = y2 + (rand() % 12 - 6);
                // Simplify leaf size calc
                int leafSize = static_cast<int>(3 * scale);
                if (leafSize > 0) fillellipse(leafX, leafY, leafSize, leafSize);
            }
        }

        // Draw flowers only at depth 1 (Tips)
        if (depth == 1 && showFlowers && scale > 0.8 && branchProgress > 0.9) {
            drawFlower(x2, y2, flowerScale);
        }

        double newLength = length * 0.7;

        // Recursive calls
        drawBranch(x2, y2, newLength, angle - 0.3, depth - 1, scale, growthProgress);
        drawBranch(x2, y2, newLength, angle + 0.3, depth - 1, scale, growthProgress);
        drawBranch(x2, y2, newLength * 0.8, angle, depth - 1, scale, growthProgress);
    }

    void drawFlower(int x, int y, double scale) {
        if (scale <= 0) return;

        // Collect all flower positions
        Point flowerPos;
        flowerPos.x = x;
        flowerPos.y = y;
        flowerPositions.push_back(flowerPos);

        int petalSize = static_cast<int>(4 * scale);

        setcolor(COLOR(255, 192, 203));
        setfillstyle(SOLID_FILL, COLOR(255, 192, 203));

        for (int i = 0; i < 5; i++) {
            double angle = i * 2 * 3.14159 / 5;
            int petalX = x + static_cast<int>(petalSize * cos(angle));
            int petalY = y + static_cast<int>(petalSize * sin(angle));
            fillellipse(petalX, petalY, petalSize, petalSize);
        }

        setcolor(YELLOW);
        setfillstyle(SOLID_FILL, YELLOW);
        fillellipse(x, y, petalSize - 1, petalSize - 1);
    }

    // Draw the sun
    void drawSun() {
        int radius = 30;
        int skyHeight = 150;
        int sunX = static_cast<int>(screenWidth * sunAngle / 3.14159);
        int sunY = static_cast<int>(skyHeight * sin(sunAngle)) + 50;

        setcolor(YELLOW);
        setfillstyle(SOLID_FILL, YELLOW);
        fillellipse(sunX, sunY, radius, radius);

        for (int i = 0; i < 12; i++) {
            double angle = i * 30 * 3.14159 / 180.0;
            int x1 = sunX + static_cast<int>((radius + 5) * cos(angle));
            int y1 = sunY + static_cast<int>((radius + 5) * sin(angle));
            int x2 = sunX + static_cast<int>((radius + 20) * cos(angle));
            int y2 = sunY + static_cast<int>((radius + 20) * sin(angle));
            line(x1, y1, x2, y2);
        }
    }

    // Draw clouds
    void drawClouds() {
        setcolor(WHITE);
        setfillstyle(SOLID_FILL, WHITE);

        for (int cloud = 0; cloud < 3; cloud++) {
            int cloudX = 100 + cloud * 200;
            int cloudY = 80 + (cloud * 17) % 50;

            for (int i = 0; i < 5; i++) {
                int circleX = cloudX + i * 25;
                int circleY = cloudY + ((i * 13) % 20 - 10);
                int radius = 20 + (i * 7) % 10;
                fillellipse(circleX, circleY, radius, radius);
            }
        }
    }

    void updateFallingSeeds() {
        for (auto& seed : fallingSeeds) {
            if (!seed.active) continue;

            // Check if seed is in the air or in the ground
            if (seed.y < groundLevel) {
                // --- IN AIR ---
                // CHANGED: Reduced gravity from 0.3 to 0.15 for slower fall
                seed.velocityY += 0.08;
                seed.y += seed.velocityY;

                // Wind effect
                seed.x += 1.0;

                // CHANGED: Rotation logic
                // Rotates continuously while falling
                seed.angle += 0.2;
            } else {
                // --- HIT GROUND ---
                // Stop rotation and horizontal movement immediately
                seed.angle = 0;  // Reset angle to 0 (flat)

                // Sinking effect
                if (seed.y < groundLevel + 20) {
                    seed.y += 0.2;  // Sink slowly
                } else {
                    seed.velocityY = 0;
                }
            }
        }
    }

    // Display phase information
    void displayPhaseInfo() {
        setcolor(WHITE);
        settextstyle(DEFAULT_FONT, HORIZ_DIR, 2);

        char title[100];
        switch (animationPhase) {
            case 0:
                sprintf(title, "Phase 1: Seed Germination");
                break;
            case 1:
                sprintf(title, "Phase 2: Seedling (Leaves)");
                break;
            case 2:
                sprintf(title, "Phase 3: Tree Growth");
                break;
            case 3:
                sprintf(title, "Phase 4: Flowering");
                break;
            case 4:
                sprintf(title, "Phase 5: Seed Dispersal");
                break;
            case 5:
                sprintf(title, "Phase 6: Cycle Reset");
                break;
        }
        outtextxy(10, 10, title);

        settextstyle(DEFAULT_FONT, HORIZ_DIR, 1);
        char msg[] = "Press ESC to exit, SPACE to restart";
        outtextxy(10, screenHeight - 20, msg);
    }

    void drawSeedlingLeaves(int x, int y, double progress) {
        // progress goes from 0 to 1
        int stemHeight = static_cast<int>(60 * progress);  // Increased from 30

        // Draw stem (this becomes the trunk)
        setcolor(LEAF_GREEN);
        setlinestyle(SOLID_LINE, 0, std::max(2, static_cast<int>(progress * 4)));
        line(x, y, x, y - stemHeight);

        // Leaves appear and grow
        if (progress > 0.2) {  // Changed from 0.3 to appear earlier
            double leafProgress = (progress - 0.2) / 0.8;
            int leafSize = static_cast<int>(20 * leafProgress);    // Increased from 15
            int leafYOffset = static_cast<int>(stemHeight * 0.5);  // Position leaves partway up stem

            setcolor(LIGHT_GREEN);
            setfillstyle(SOLID_FILL, LIGHT_GREEN);

            // Left leaf - angled outward
            fillellipse(x - leafSize, y - leafYOffset, leafSize, static_cast<int>(leafSize * 0.6));

            // Right leaf - angled outward
            fillellipse(x + leafSize, y - leafYOffset, leafSize, static_cast<int>(leafSize * 0.6));
        }
    }

   public:
    AnimatedTreeDrawer()
        : screenWidth(800),
          screenHeight(600),
          groundLevel(480),
          seedX(400),
          seedY(groundLevel + 25),
          sunAngle(0.0),
          treeGrowthScale(0.0),
          flowerScale(0.0),
          showFlowers(false),
          animationPhase(0),
          phaseTimer(0),
          rightmostBranchX(0),
          rightmostBranchY(0),
          zoomScale(1.0),
          cameraOffsetX(0),
          cameraOffsetY(0) {}

    void initialize() {
        lastTime = std::chrono::steady_clock::now();
        initwindow(screenWidth, screenHeight, "Animated Tree Life Cycle");
        setbkcolor(SKY_BLUE);
        cleardevice();
        setactivepage(0);
        setvisualpage(0);
    }

    void resetAnimation() {
        treeGrowthScale = 0.0;
        flowerScale = 0.0;
        showFlowers = false;
        animationPhase = 0;
        phaseTimer = 0;
        fallingSeeds.clear();
        seedX = screenWidth / 2;
        seedY = groundLevel + 25;
        zoomScale = 1.0;
        cameraOffsetX = 0;
        cameraOffsetY = 0;
        flowerPositions.clear();
        rightmostBranchX = 0;
        rightmostBranchY = 0;
        fallingSeeds.clear();
    }

    void update() {
        phaseTimer += 1;

        switch (animationPhase) {
            case 0:  // Seed germination (0-40 frames)
                if (phaseTimer < 40) {
                    treeGrowthScale = 0.0;
                } else {
                    animationPhase = 1;
                    phaseTimer = 0;
                }
                break;

            case 1: {  // Leaf phase (0-60 frames) - grows smoothly
                if (phaseTimer < 60) {
                    // Don't set to 0, let it transition smoothly
                    treeGrowthScale = std::min(0.15, phaseTimer / 400.0);  // Very gradual start
                } else {
                    animationPhase = 2;
                    phaseTimer = 0;
                }
                break;
            }

            case 2: {  // Tree growth (0-100 frames)
                if (phaseTimer < 100) {
                    treeGrowthScale = 0.15 + (phaseTimer / 100.0) * 0.85;  // Continue from 0.15
                } else {
                    animationPhase = 3;
                    phaseTimer = 0;
                    showFlowers = true;
                }
                break;
            }

            case 3: {  // Flowering
                if (phaseTimer < 25) {
                    flowerScale = phaseTimer / 25.0;
                } else {
                    animationPhase = 4;
                    phaseTimer = 0;

                    Seed newSeed;
                    // Ensure the seed spawns on the RIGHT side of the tree
                    // We use seedX (the trunk) + an offset
                    newSeed.x = seedX + 60;   // Offset to the right
                    newSeed.y = seedY - 180;  // Up in the branches

                    newSeed.angle = 0;
                    newSeed.velocityY = 0;
                    newSeed.active = true;
                    fallingSeeds.clear();
                    fallingSeeds.push_back(newSeed);

                    // Note: Don't set zoomScale = 8.0 here anymore!
                    // Case 4 will now handle the slow zoom-in.
                }
                break;
            }

            case 4: {  // Falling phase
                updateFallingSeeds();

                if (!fallingSeeds.empty()) {
                    Seed& s = fallingSeeds[0];

                    // 1. GRADUAL ZOOM: Slowly increase zoom from 1.0 to 8.0
                    if (zoomScale < 8.0) {
                        zoomScale += 0.05;  // Adjust this value to change zoom speed
                    }

                    // 2. TARGET CALCULATION: Where the camera *wants* to be to center the seed
                    double targetOffsetX = (screenWidth / 2.0 / zoomScale) - s.x;
                    double targetOffsetY = (screenHeight / 2.0 / zoomScale) - s.y;

                    // 3. INTERPOLATION (The "Slow" part):
                    // Move 5% of the way to the target every frame.
                    // This creates the "panning" effect where the tree moves out to the left.
                    cameraOffsetX += (targetOffsetX - cameraOffsetX) * 0.05;
                    cameraOffsetY += (targetOffsetY - cameraOffsetY) * 0.05;

                    // Transition logic for when the seed hits the ground
                    if (s.y >= groundLevel + 20) {
                        if (phaseTimer > 40) {
                            animationPhase = 5;
                            phaseTimer = 0;
                        }
                    }
                }
                break;
            }

            case 5: {                    // Seamless Reset
                if (phaseTimer < 100) {  // Increased time for slower zoom
                    // Fade out old tree
                    treeGrowthScale = std::max(0.0, 1.0 - (phaseTimer / 50.0));
                    flowerScale = std::max(0.0, 1.0 - (phaseTimer / 50.0));

                    // Zoom out from 8x back to 1x
                    double zoomProgress = phaseTimer / 100.0;
                    zoomScale = 8.0 - (zoomProgress * 7.0);

                    // CHANGED: Camera Tracking Logic
                    // We want to keep the NEW seed (seedX, seedY) in the center of the screen
                    // as we zoom out.
                    // Formula: Center - (WorldPos * Zoom)
                    cameraOffsetX = (screenWidth / 2) / zoomScale - seedX;
                    cameraOffsetY = (screenHeight / 2) / zoomScale - seedY;

                    // Adjust offsets to pixel coordinates
                    cameraOffsetX = screenWidth / 2 - static_cast<int>(seedX * zoomScale);
                    cameraOffsetY = screenHeight / 2 - static_cast<int>(seedY * zoomScale);

                } else {
                    // RESET
                    resetAnimation();

                    // HACK for seamless loop:
                    // resetAnimation sets seedX to 400 (center).
                    // Since we just finished zooming out with the camera centered on the
                    // old seed (wherever it landed), snapping seedX to 400 now
                    // is invisible because the screen is just sky/ground (uniform).
                }
                break;
            }
        }
        auto currentTime = std::chrono::steady_clock::now();
        double elapsedSeconds = std::chrono::duration<double>(currentTime - lastTime).count();
        lastTime = currentTime;

        sunAngle += 0.5 * elapsedSeconds;  // radians per second
        if (sunAngle > 2 * 3.14159) sunAngle -= 2 * 3.14159;
    }

    void render() {
        setactivepage(1 - getactivepage());

        // (Keep your sky color logic here...)
        int r = 100 - static_cast<int>(50 * -sin(sunAngle));
        int g = 170 - static_cast<int>(100 * -sin(sunAngle));
        int b = 200 - static_cast<int>(80 * -sin(sunAngle));
        r = std::max(0, std::min(255, r));
        g = std::max(0, std::min(255, g));
        b = std::max(0, std::min(255, b));

        setbkcolor(COLOR(r, g, b));
        cleardevice();

        drawSun();
        drawClouds();

        // (Keep your transform/ground logic here...)
        int drawOffsetX = cameraOffsetX;
        int drawOffsetY = cameraOffsetY;
        int transformedGroundLevel = groundLevel + drawOffsetY;
        int originalGroundLevel = groundLevel;
        groundLevel = transformedGroundLevel;
        drawSoil();
        groundLevel = originalGroundLevel;

        // Draw seed underground (Keep your logic here...)
        if (animationPhase <= 1) {
            double seedScale = 1.0 + (animationPhase == 0 ? phaseTimer / 20.0 : 2.0);
            int seedDrawX = static_cast<int>((seedX + drawOffsetX) * zoomScale - (zoomScale - 1.0) * screenWidth / 2);
            int seedDrawY = static_cast<int>((seedY + drawOffsetY) * zoomScale - (zoomScale - 1.0) * screenHeight / 2);
            drawSeed(seedDrawX, seedDrawY, 0, seedScale * zoomScale);

            if (animationPhase == 0 && phaseTimer > 20) {
                setcolor(LIGHT_GREEN);
                double sproutProgress = (phaseTimer - 20) / 20.0;
                int sproutLength = static_cast<int>(sproutProgress * 20 * zoomScale);
                line(seedDrawX, seedDrawY, seedDrawX, seedDrawY - sproutLength);
            }
        }

        // Draw Seedling (Phase 1)
        if (animationPhase == 1) {
            double leafProgress = phaseTimer / 60.0;
            int leafDrawX = static_cast<int>((seedX + drawOffsetX) * zoomScale - (zoomScale - 1.0) * screenWidth / 2);
            double baseY = seedY + leafProgress * (groundLevel - seedY);
            int leafDrawY = static_cast<int>((baseY + drawOffsetY) * zoomScale - (zoomScale - 1.0) * screenHeight / 2);
            drawSeedlingLeaves(leafDrawX, leafDrawY, leafProgress);
        }

        // TREE DRAWING (Optimized)
        if (animationPhase >= 2 || (animationPhase == 1 && phaseTimer > 50)) {
            double blendFactor = 1.0;
            if (animationPhase == 1) {
                blendFactor = (phaseTimer - 50) / 10.0;
            }
            int startX = static_cast<int>((seedX + cameraOffsetX) * zoomScale);
            int startY = static_cast<int>((groundLevel + cameraOffsetY) * zoomScale);

            int trunkLength = static_cast<int>(150 * zoomScale);
            double initialAngle = 3.14159 / 2;

            if (treeGrowthScale > 0.01) {
                drawBranch(startX, startY, trunkLength, initialAngle, 6, treeGrowthScale * blendFactor, treeGrowthScale * blendFactor);
            }
        }

        // Draw falling seeds (Keep your logic here...)
        for (const auto& seed : fallingSeeds) {
            if (seed.active) {
                int centerX = screenWidth / 2;
                int centerY = screenHeight / 2;
                drawSeed(centerX, centerY, seed.angle, zoomScale * 2.0);
            }
        }

        displayPhaseInfo();
        setvisualpage(getactivepage());
    }

    void run() {
        initialize();

        while (true) {
            // Check for keyboard input
            if (kbhit()) {
                char key = getch();
                if (key == 27) break;              // ESC to exit
                if (key == ' ') resetAnimation();  // SPACE to restart
            }

            // Update animation state
            update();

            // Render frame
            render();

            // Control frame rate (~30 FPS)
            // Sleep(33);  // Windows Sleep uses milliseconds
            delay(33);
        }

        closegraph();
    }
};

int main() {
    AnimatedTreeDrawer drawer;
    drawer.run();

    return 0;
}
