#include "history_chart.h"
#include "ui_history_chart.h"

HistoryChart::~HistoryChart()
{
    delete ui;
}

HistoryChart::HistoryChart(const QString &title, const int &seriesCount, QCategoryAxis* categoriAxisY, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HistoryChart),
    mTitle(title),
    mSeriesCount(seriesCount),
    mChartView(new QChartView(this)),
    mChart(mChartView->chart()),
    mAxisY(nullptr),
    mValueAxisX(nullptr),
    mValueAxisY(nullptr)
{
    ui->setupUi(this);

    init();

    if (categoriAxisY) {
        mAxisY = categoriAxisY;
        mAxisY->setLabelsPosition(QCategoryAxis::AxisLabelsPositionOnValue);
        mChart->addAxis(mAxisY, Qt::AlignLeft);
        for (int i = 0; i < seriesCount; ++i) {
            // the category axis takes over the Y side from the default value axis
            if (mValueAxisY)
                mSeriesList.at(i)->detachAxis(mValueAxisY);

            mSeriesList.at(i)->attachAxis(mAxisY);
        }
    }
}

void HistoryChart::init()
{
    ui->lblHistoryTitle->setText(mTitle);

    // add series to chart
    for (int i = 0; i < mSeriesCount; i++) {
        mSeriesList.append(new QSplineSeries);
        mChart->addSeries(mSeriesList.at(i));
    }

    mChartView->setRenderHint(QPainter::Antialiasing);

    QList<int> colors = {
        0x2ecc71, 0xe74c3c, 0x3498db, 0xf1c40f, 0xe67e22,
        0x1abc9c, 0x9b59b6, 0x34495e, 0xd35400, 0xc0392b,
        0x8e44ad, 0xFF8F00, 0xEF6C00, 0x4E342E, 0x424242,
        0x5499C7, 0x58D68D, 0xCD6155, 0xF5B041, 0x566573
    };
    // set colors
    for (int i = 0; i < mSeriesList.count(); ++i) {
        dynamic_cast<QSplineSeries*>(mChart->series().at(i))->setColor(QColor(colors.at(i)));
    }

    // Chart Settings
    mChart->createDefaultAxes();

    // QChart::axisX()/axisY() were removed in Qt6, so keep hold of the axes that
    // createDefaultAxes() attached to the series.
    if (!mSeriesList.isEmpty()) {
        for (QAbstractAxis *axis : mSeriesList.first()->attachedAxes()) {
            if (auto *valueAxis = qobject_cast<QValueAxis *>(axis)) {
                if (axis->alignment() == Qt::AlignBottom)
                    mValueAxisX = valueAxis;
                else if (axis->alignment() == Qt::AlignLeft)
                    mValueAxisY = valueAxis;
            }
        }
    }

    if (mValueAxisX) {
        mValueAxisX->setRange(0, 60);
        mValueAxisX->setReverse(true);
    }

    mChart->setContentsMargins(-11, -11, -11, -11);
    mChart->setMargins(QMargins(20, 0, 10, 10));
    ui->layoutHistoryChart->addWidget(mChartView, 1, 0, 1, 3);

    // theme changed
    connect(SignalMapper::ins(), &SignalMapper::sigChangedAppTheme, [=] {
        QString chartLabelColor = AppManager::ins()->getStyleValues()->value("@chartLabelColor").toString();
        QString chartGridColor = AppManager::ins()->getStyleValues()->value("@chartGridColor").toString();
        QString historyChartBackground = AppManager::ins()->getStyleValues()->value("@historyChartBackgroundColor").toString();

        if (mValueAxisX) {
            mValueAxisX->setLabelsColor(chartLabelColor);
            mValueAxisX->setGridLineColor(chartGridColor);
        }

        if (QAbstractAxis *axisY = currentAxisY()) {
            axisY->setLabelsColor(chartLabelColor);
            axisY->setGridLineColor(chartGridColor);
        }

        mChart->setBackgroundBrush(QColor(historyChartBackground));
        mChart->legend()->setLabelColor(chartLabelColor);
    });
}

void HistoryChart::setYMax(const int &value)
{
    if (QAbstractAxis *axisY = currentAxisY())
        axisY->setRange(0, value);
}

QCategoryAxis *HistoryChart::getAxisY()
{
    return mAxisY;
}

void HistoryChart::setCategoryAxisYLabels()
{
    if (mAxisY) {
        for (const QString &label : mAxisY->categoriesLabels()){
            mAxisY->remove(label);
        }

        for (int i = 1; i < 5; ++i) {
            mAxisY->append(FormatUtil::formatBytes((mAxisY->max()/4)*i), (mAxisY->max()/4)*i);
        }
    }
}

QVector<QSplineSeries*> HistoryChart::getSeriesList() const
{
    return mSeriesList;
}

void HistoryChart::setSeriesList(const QVector<QSplineSeries *> &seriesList)
{
    for (int i = 0; i < seriesList.count(); ++i) {
        mChart->series().replace(0, seriesList.at(i));
    }

    mChartView->repaint();
}

void HistoryChart::on_checkHistoryTitle_clicked(bool checked)
{
    QLayout *charts = topLevelWidget()->findChild<QWidget*>("charts")->layout();

    for (int i = 0; i < charts->count(); ++i) {
        charts->itemAt(i)->widget()->setVisible(! checked);
    }

    show();
}
