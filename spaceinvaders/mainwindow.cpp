#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QPainter>
#include <QRandomGenerator>
#include <QFont>
#include <algorithm>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , lives(3)
    , score(0)
    , level(1)
    , highScore(0)
    , gameOver(false)
    , moveLeft(false)
    , moveRight(false)
    , gameState(GameState::Menu)
    , difficulty(Difficulty::Easy)
    , menuSelectionIndex(0)
    , settingsSelectionIndex(0)
    , gameOverSelectionIndex(0)
    , playerName("Player")
{
    ui->setupUi(this);

    setFixedSize(800, 600);
    setWindowTitle("Space Invaders");

    // Load sprites from Qt resources
    backgroundPixmap   = QPixmap(":/images/space bg.jpg");
    playerPixmap       = QPixmap(":/images/player_ship_clean.png");
    enemyPixmap        = QPixmap(":/images/enemy_ship_clean.png");
    playerLaserPixmap  = QPixmap(":/images/player_laser_clean.png");

    loadHighScore();
    loadPlayerName();
    initGame();

    // First run: go straight to name entry if still default
    if (playerName == "Player") {
        nameInputBuffer = playerName;
        gameState = GameState::NameEntry;
    } else {
        gameState = GameState::Menu;
    }

    // Game loop ~60 FPS
    connect(&gameTimer, &QTimer::timeout, this, &MainWindow::updateGame);
    gameTimer.start(16);

    // Enemy movement + shooting
    connect(&enemyMoveTimer, &QTimer::timeout, this, &MainWindow::moveEnemiesRandom);
    connect(&enemyShootTimer, &QTimer::timeout, this, &MainWindow::enemiesShoot);

    applyDifficulty();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// --- High score + name persistence ---

void MainWindow::loadHighScore()
{
    QSettings settings("MyCompany", "SpaceInvaders");
    highScore = settings.value("highScore", 0).toInt();
}

void MainWindow::saveHighScore()
{
    QSettings settings("MyCompany", "SpaceInvaders");
    settings.setValue("highScore", highScore);
}

void MainWindow::loadPlayerName()
{
    QSettings settings("MyCompany", "SpaceInvaders");
    playerName = settings.value("playerName", "Player").toString();
}

void MainWindow::savePlayerName()
{
    QSettings settings("MyCompany", "SpaceInvaders");
    settings.setValue("playerName", playerName);
}

// --- Game setup ---

void MainWindow::initGame()
{
    int w = width();
    int h = height();
    int playerWidth = 48;
    int playerHeight = 48;

    playerRect = QRect((w - playerWidth) / 2, h - 90, playerWidth, playerHeight);

    playerLasers.clear();
    enemyBullets.clear();
    enemies.clear();

    setupLevel();
}

void MainWindow::setupLevel()
{
    enemies.clear();

    int rows = 3;
    int cols = 7;
    int enemyWidth = 36;
    int enemyHeight = 36;
    int topMargin = 60;
    int leftMargin = 80;
    int hSpacing = 80;
    int vSpacing = 70;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Enemy e;
            int x = leftMargin + c * hSpacing;
            int y = topMargin + r * vSpacing;
            e.rect = QRect(x, y, enemyWidth, enemyHeight);
            e.alive = true;
            enemies.push_back(e);
        }
    }

    applyDifficulty();
}

void MainWindow::resetForNextLevel()
{
    level++;
    playerLasers.clear();
    enemyBullets.clear();
    setupLevel();
    gameOver = false;
    gameState = GameState::Playing;
}

void MainWindow::resetGameCompletely()
{
    level = 1;
    score = 0;
    lives = 3;
    gameOver = false;
    playerLasers.clear();
    enemyBullets.clear();
    setupLevel();
    applyDifficulty();
    gameState = GameState::Playing;
}

// Adjust difficulty (both faster)
void MainWindow::applyDifficulty()
{
    int baseShootInterval;
    int baseMoveInterval;

    if (difficulty == Difficulty::Easy) {
        baseShootInterval = 1600;
        baseMoveInterval  = 550;
    } else { // Hard
        baseShootInterval = 1100;
        baseMoveInterval  = 400;
    }

    int shootInterval = std::max(350, baseShootInterval - (level - 1) * 220);
    int moveInterval  = std::max(150, baseMoveInterval  - (level - 1) * 70);

    enemyShootTimer.start(shootInterval);
    enemyMoveTimer.start(moveInterval);
}

// ---- Input handling ----

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    int key = event->key();

    // --- NAME ENTRY STATE ---
    if (gameState == GameState::NameEntry) {
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            playerName = nameInputBuffer.isEmpty() ? QString("Player") : nameInputBuffer;
            savePlayerName();
            gameState = GameState::Menu;
            update();
            return;
        } else if (key == Qt::Key_Escape) {
            gameState = GameState::Settings;
            update();
            return;
        } else if (key == Qt::Key_Backspace) {
            if (!nameInputBuffer.isEmpty())
                nameInputBuffer.chop(1);
        } else {
            QString text = event->text();
            if (!text.isEmpty() && nameInputBuffer.size() < 12) {
                QChar ch = text[0];
                ushort uc = ch.unicode();
                if (uc >= 32 && uc < 127) {   // printable ASCII
                    nameInputBuffer.append(ch);
                }
            }
        }
        update();
        return;
    }

    // --- MENU STATE ---
    if (gameState == GameState::Menu) {
        if (key == Qt::Key_Up || key == Qt::Key_Down) {
            menuSelectionIndex = (menuSelectionIndex + 1) % 2; // 0-1
        } else if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
            if (menuSelectionIndex == 0) {
                resetGameCompletely();
            } else if (menuSelectionIndex == 1) {
                gameState = GameState::Settings;
            }
        }
        update();
        return;
    }

    // --- SETTINGS STATE ---
    if (gameState == GameState::Settings) {
        if (key == Qt::Key_Up || key == Qt::Key_Down) {
            settingsSelectionIndex = (settingsSelectionIndex == 0) ? 1 : 0;
        } else if (key == Qt::Key_Escape) {
            gameState = GameState::Menu;
        } else if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
            if (settingsSelectionIndex == 0) {
                difficulty = (difficulty == Difficulty::Easy) ? Difficulty::Hard : Difficulty::Easy;
                applyDifficulty();
            } else if (settingsSelectionIndex == 1) {
                nameInputBuffer = playerName;
                gameState = GameState::NameEntry;
            }
        }
        update();
        return;
    }

    // --- GAME OVER STATE ---
    if (gameState == GameState::GameOver) {
        if (key == Qt::Key_Up || key == Qt::Key_Down) {
            gameOverSelectionIndex = (gameOverSelectionIndex == 0) ? 1 : 0;
        } else if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
            if (gameOverSelectionIndex == 0) {
                resetGameCompletely();
            } else {
                gameState = GameState::Menu;
            }
        }
        update();
        return;
    }

    // --- PLAYING STATE ---
    if (gameState == GameState::Playing) {
        if (key == Qt::Key_Left || key == Qt::Key_A) {
            moveLeft = true;
        } else if (key == Qt::Key_Right || key == Qt::Key_D) {
            moveRight = true;
        } else if (key == Qt::Key_Space) {
            // Create an animated laser beam
            PlayerLaser laser;
            int width = 10;
            int height = 36;
            int cx = playerRect.center().x();
            int top = playerRect.top() - height;
            laser.rect = QRect(cx - width / 2, top, width, height);
            laser.frame = 0;
            playerLasers.push_back(laser);
        }
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    int key = event->key();

    if (gameState == GameState::Playing) {
        if (key == Qt::Key_Left || key == Qt::Key_A) {
            moveLeft = false;
        } else if (key == Qt::Key_Right || key == Qt::Key_D) {
            moveRight = false;
        }
    }

    QMainWindow::keyReleaseEvent(event);
}

// ---- Game loop ----

void MainWindow::updateGame()
{
    if (gameState != GameState::Playing) {
        update();
        return;
    }

    int speed = 7;
    if (moveLeft) {
        playerRect.translate(-speed, 0);
    }
    if (moveRight) {
        playerRect.translate(speed, 0);
    }

    if (playerRect.left() < 0) playerRect.moveLeft(0);
    if (playerRect.right() > width()) playerRect.moveRight(width());

    updateBullets();
    handleCollisions();
    checkLevelComplete();

    update();
}

void MainWindow::updateBullets()
{
    // Player lasers go up and animate
    for (int i = 0; i < playerLasers.size(); ++i) {
        playerLasers[i].rect.translate(0, -14);
        playerLasers[i].frame++;
    }
    playerLasers.erase(
        std::remove_if(playerLasers.begin(), playerLasers.end(),
                       [this](const PlayerLaser &l) { return l.rect.bottom() < 0; }),
        playerLasers.end()
        );

    // Enemy bullets go down
    for (int i = 0; i < enemyBullets.size(); ++i) {
        enemyBullets[i].translate(0, 9);
    }
    enemyBullets.erase(
        std::remove_if(enemyBullets.begin(), enemyBullets.end(),
                       [this](const QRect &b) { return b.top() > height(); }),
        enemyBullets.end()
        );
}

void MainWindow::handleCollisions()
{
    if (gameState != GameState::Playing)
        return;

    // Player lasers vs enemies
    for (int i = 0; i < playerLasers.size(); ) {
        bool laserRemoved = false;
        for (Enemy &e : enemies) {
            if (e.alive && e.rect.intersects(playerLasers[i].rect)) {
                e.alive = false;
                score += 10;
                playerLasers.remove(i);
                laserRemoved = true;
                break;
            }
        }
        if (!laserRemoved) {
            ++i;
        }
    }

    // Enemy bullets vs player
    for (int i = 0; i < enemyBullets.size(); ) {
        if (enemyBullets[i].intersects(playerRect)) {
            enemyBullets.remove(i);
            lives--;

            if (lives <= 0) {
                gameOver = true;
                gameState = GameState::GameOver;

                // update single high score
                if (score > highScore) {
                    highScore = score;
                    saveHighScore();
                }
            }
        } else {
            ++i;
        }
    }
}

void MainWindow::checkLevelComplete()
{
    if (gameState != GameState::Playing)
        return;

    bool anyAlive = false;
    for (const Enemy &e : enemies) {
        if (e.alive) {
            anyAlive = true;
            break;
        }
    }

    if (!anyAlive) {
        resetForNextLevel();
    }
}

// ---- Enemy behavior ----

void MainWindow::moveEnemiesRandom()
{
    if (gameState != GameState::Playing)
        return;

    int topLimit = 40;
    int bottomLimit = 230; // upper section
    int leftLimit = 20;
    int rightLimit = width() - 60;

    for (Enemy &e : enemies) {
        if (!e.alive) continue;

        int dx = QRandomGenerator::global()->bounded(-20, 21);
        int dy = QRandomGenerator::global()->bounded(-10, 11);

        e.rect.translate(dx, dy);

        if (e.rect.left() < leftLimit) e.rect.moveLeft(leftLimit);
        if (e.rect.right() > rightLimit) e.rect.moveRight(rightLimit);
        if (e.rect.top() < topLimit) e.rect.moveTop(topLimit);
        if (e.rect.bottom() > bottomLimit) e.rect.moveBottom(bottomLimit);
    }
}

void MainWindow::enemiesShoot()
{
    if (gameState != GameState::Playing)
        return;

    QVector<int> aliveIndices;
    for (int i = 0; i < enemies.size(); ++i) {
        if (enemies[i].alive) {
            aliveIndices.push_back(i);
        }
    }

    if (aliveIndices.isEmpty())
        return;

    int idx = aliveIndices[QRandomGenerator::global()->bounded(aliveIndices.size())];
    Enemy &shooter = enemies[idx];

    QRect bullet(shooter.rect.center().x() - 3, shooter.rect.bottom(), 6, 16);
    enemyBullets.push_back(bullet);
}

// ---- Rendering ----

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // High-res looking background: scale smoothly to window size
    if (!backgroundPixmap.isNull()) {
        QPixmap scaled = backgroundPixmap.scaled(size(),
                                                 Qt::IgnoreAspectRatio,
                                                 Qt::SmoothTransformation);
        painter.drawPixmap(0, 0, scaled);
    } else {
        painter.fillRect(rect(), Qt::black);
    }

    // MENU
    if (gameState == GameState::Menu) {
        painter.setPen(Qt::white);
        QFont titleFont = painter.font();
        titleFont.setPointSize(28);
        titleFont.setBold(true);
        painter.setFont(titleFont);

        painter.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop, "Space Invaders");

        QFont menuFont = painter.font();
        menuFont.setPointSize(16);
        menuFont.setBold(false);
        painter.setFont(menuFont);

        int startY = height() / 2 - 40;
        QString options[2] = { "Start Game", "Game Settings" };

        for (int i = 0; i < 2; ++i) {
            QString text = (i == menuSelectionIndex ? "> " : "  ") + options[i];
            painter.drawText(0, startY + i * 30, width(), 30, Qt::AlignHCenter, text);
        }

        painter.drawText(0, height() - 60, width(), 20,
                         Qt::AlignHCenter, "Use Up/Down + Enter/Space");

        painter.drawText(10, height() - 30, QString("High Score: %1").arg(highScore));
        return;
    }

    // SETTINGS
    if (gameState == GameState::Settings) {
        painter.setPen(Qt::white);
        QFont titleFont = painter.font();
        titleFont.setPointSize(24);
        titleFont.setBold(true);
        painter.setFont(titleFont);

        painter.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop, "Game Settings");

        QFont menuFont = painter.font();
        menuFont.setPointSize(16);
        menuFont.setBold(false);
        painter.setFont(menuFont);

        QString diffText = (difficulty == Difficulty::Easy) ? "Easy" : "Hard";

        QString lines[2] = {
            QString("Difficulty: %1").arg(diffText),
            QString("Player Name: %1").arg(playerName)
        };

        int startY = height() / 2 - 20;
        for (int i = 0; i < 2; ++i) {
            QString text = (i == settingsSelectionIndex ? "> " : "  ") + lines[i];
            painter.drawText(0, startY + i * 30, width(), 30, Qt::AlignHCenter, text);
        }

        painter.drawText(0, height() - 60, width(), 20,
                         Qt::AlignHCenter,
                         "Up/Down to select | Enter to toggle/edit | Esc to menu");

        return;
    }

    // NAME ENTRY
    if (gameState == GameState::NameEntry) {
        painter.setPen(Qt::white);
        QFont titleFont = painter.font();
        titleFont.setPointSize(24);
        titleFont.setBold(true);
        painter.setFont(titleFont);

        painter.drawText(rect(), Qt::AlignHCenter | Qt::AlignTop, "Enter Name");

        QFont textFont = painter.font();
        textFont.setPointSize(18);
        textFont.setBold(false);
        painter.setFont(textFont);

        painter.drawText(0, height() / 2, width(), 30,
                         Qt::AlignHCenter,
                         QString("> %1_").arg(nameInputBuffer));

        painter.drawText(0, height() - 60, width(), 20,
                         Qt::AlignHCenter,
                         "Type your name, Enter to confirm, Esc to cancel");
        return;
    }

    // GAMEPLAY RENDERING

    // Player
    if (!playerPixmap.isNull()) {
        painter.drawPixmap(playerRect, playerPixmap);
    } else {
        painter.setBrush(Qt::green);
        painter.setPen(Qt::NoPen);
        painter.drawRect(playerRect);
    }

    // Enemies
    for (const Enemy &e : enemies) {
        if (!e.alive) continue;
        if (!enemyPixmap.isNull()) {
            painter.drawPixmap(e.rect, enemyPixmap);
        } else {
            painter.setBrush(Qt::red);
            painter.setPen(Qt::NoPen);
            painter.drawRect(e.rect);
        }
    }

    // Player LASERS with simple beam animation (pulse wider)
    for (const PlayerLaser &l : playerLasers) {
        if (!playerLaserPixmap.isNull()) {
            int pulse = (l.frame % 6 < 3) ? 2 : 0; // widen every few frames
            QRect animated = l.rect.adjusted(-pulse, 0, pulse, 0);
            painter.drawPixmap(animated, playerLaserPixmap);
        } else {
            painter.setBrush(Qt::yellow);
            painter.setPen(Qt::NoPen);
            painter.drawRect(l.rect);
        }
    }

    // Enemy bullets
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    for (const QRect &b : enemyBullets) {
        painter.drawRect(b);
    }

    // HUD
    painter.setPen(Qt::white);
    painter.setBrush(Qt::NoBrush);
    QFont f = painter.font();
    f.setPointSize(12);
    painter.setFont(f);

    painter.drawText(10, 20, QString("Score: %1").arg(score));
    painter.drawText(10, 40, QString("Lives: %1").arg(lives));
    painter.drawText(10, 60, QString("Level: %1").arg(level));
    painter.drawText(width() - 220, 20, QString("High Score: %1").arg(highScore));
    painter.drawText(width() - 220, 40, QString("Player: %1").arg(playerName));

    // GAME OVER overlay
    if (gameState == GameState::GameOver) {
        painter.setPen(Qt::white);
        QFont big = painter.font();
        big.setPointSize(26);
        big.setBold(true);
        painter.setFont(big);

        painter.drawText(rect(), Qt::AlignHCenter | Qt::AlignVCenter, "GAME OVER");

        QFont small = painter.font();
        small.setPointSize(16);
        small.setBold(false);
        painter.setFont(small);

        QString options[2] = { "Restart", "End Game" };
        int startY = height() / 2 + 40;

        for (int i = 0; i < 2; ++i) {
            QString text = (i == gameOverSelectionIndex ? "> " : "  ") + options[i];
            painter.drawText(0, startY + i * 30, width(), 30, Qt::AlignHCenter, text);
        }

        painter.drawText(0, height() - 40, width(), 20,
                         Qt::AlignHCenter, "Use Up/Down + Enter/Space");
    }
}
