#include "MainWindow.h"
#include "../Core/Theme.h"

MainWindow::MainWindow(QObject *parent, QSettings *_iniSettings) : parentMain(parent), iniSettings(_iniSettings)
{
    titleBar = new Titlebar(this);

    setObjectName("window");
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowIcon(QIcon(Global::getQrcPath("icon2.png")));
    setFixedWidth(500);

    if (iniSettings->value("Window/Position").toPoint().isNull())
        move(30,30);
    else
        move(iniSettings->value("Window/Position").toPoint());
}

MainWindow::~MainWindow()
{
    Database::setValue("Current", "mode", centralWidget->mode);
    iniSettings->setValue("Window/Position", pos());
    delete centralWidget;
}

void MainWindow::initWindow(Widget *widget)
{
    centralWidget = widget;

    mainLayout = new QVBoxLayout;
    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(widget, 1);

    setLayout(mainLayout);
    show();
}

QString &MainWindow::currentMode()
{
    return centralWidget->mode;
}

int MainWindow::marginLayout() const
{
    return mainLayout->margin();
}

int MainWindow::spacingLayout() const
{
    return mainLayout->spacing();
}

void MainWindow::setMarginLayout(int margin)
{
    mainLayout->setMargin(margin);
}

void MainWindow::setSpacingLayout(int spacing)
{
    mainLayout->setSpacing(spacing);
}

//================================================================================================================
// public slots
//================================================================================================================

void MainWindow::setWindowTitle(const QString &title)
{
    if (titleBar)
        titleBar->label->setText(title);

    QWidget::setWindowTitle(title);
}
