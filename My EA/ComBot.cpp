#include <Trade\Trade.mqh>
#include <ChartObjects\ChartObjectsTxtControls.mqh>

CTrade Trade;
CChartObjectLabel lblTotalBuyLot, lblTotalSellLot, lblTotalLots;
CChartObjectText txtTimeCountDown;

// Input parameters
input double LotSize = 0.01;        // Khối lượng từng lệnh
input double TakeProfitUSD = 1.5;   // Mức lợi nhuận mục tiêu (USD)
input double MaxDrawdownUSD = 50.0; // Mức thua lỗ tối đa (USD)

// Global variables
datetime lastCandleTime = 0;
bool alertSent = false;

// Variables for one-time tick calculation
double g_buyProfit = 0.0;
double g_sellProfit = 0.0;
double g_buyLots = 0.0;
double g_sellLots = 0.0;
double g_totalSwap = 0.0;

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
{
    EventSetTimer(1); // Giữ Timer chỉ để update UI 1 giây/lần cho mượt

    int x = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS) - 200;

    if (!CreateLable(lblTotalBuyLot, "TotalBuyLot", "Total Buy Lots: 0.00", x, 30))
        return (INIT_FAILED);
    if (!CreateLable(lblTotalSellLot, "TotalSellLot", "Total Sell Lots: 0.00", x, 60))
        return (INIT_FAILED);
    if (!CreateLable(lblTotalLots, "TotalLots", "Total Lots: 0.00", x, 90))
        return (INIT_FAILED);

    if (!CreateText(txtTimeCountDown, "TimeCountDown", "Countdown:  s"))
        return (INIT_FAILED);
    ChartRedraw(0);
    return (INIT_SUCCEEDED);
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                 |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
    EventKillTimer();
}

//+------------------------------------------------------------------+
//| UI Update function (Chỉ update text hiển thị)                    |
//+------------------------------------------------------------------+
void OnTimer()
{
    lblTotalBuyLot.Description("Total Buy Lots: " + DoubleToString(g_buyLots, 2));
    lblTotalSellLot.Description("Total Sell Lots: " + DoubleToString(g_sellLots, 2));
    lblTotalLots.Description("Daily Lots: " + DoubleToString(GetDailyTradedLots(), 2));

    // Cập nhật countdown đến nến tiếp theo
    datetime currentTime = TimeCurrent();
    datetime nextCandleTime = iTime(_Symbol, PERIOD_CURRENT, 0) + PeriodSeconds(PERIOD_CURRENT);
    int secondsToNextCandle = (int)(nextCandleTime - currentTime - 1);

    txtTimeCountDown.Description("    " + IntegerToString(secondsToNextCandle) + "s");
    double currentPrice = iClose(_Symbol, PERIOD_CURRENT, 0);

    txtTimeCountDown.Time(0, nextCandleTime);
    txtTimeCountDown.Price(0, currentPrice);

    ChartRedraw();
}

//+------------------------------------------------------------------+
//| Expert tick function                                             |
//+------------------------------------------------------------------+
void OnTick()
{
    // 1. Tính toán tất cả trạng thái lệnh TRONG 1 VÒNG LẶP DUY NHẤT
    CalculatePositionStats();

    double totalNetProfit = g_buyProfit + g_sellProfit + g_totalSwap;

    // 2. Kiểm tra điều kiện chốt lời / cắt lỗ tổng
    if (totalNetProfit >= TakeProfitUSD || totalNetProfit <= -MaxDrawdownUSD)
    {
        CloseAllPositions();
        alertSent = false; // Reset cảnh báo sau khi đóng lệnh
        return;            // Dừng xử lý tick hiện tại sau khi đóng lệnh
    }

    // 3. Kiểm tra cảnh báo
    if (totalNetProfit <= -20.0)
    {
        if (!alertSent)
        {
            Alert("Cảnh báo: Lỗ đã đạt -20 USD! Hãy kiểm tra lại chiến lược.");
            alertSent = true;
        }
    }

    datetime currentCandleTime = iTime(_Symbol, PERIOD_M1, 0);
    if (currentCandleTime != lastCandleTime)
    {
        lastCandleTime = currentCandleTime;
        TradeCom();
    }

    HedgePositions();
}

//+------------------------------------------------------------------+
//| Core Function: Duyệt lệnh 1 lần để lấy toàn bộ thông tin         |
//+------------------------------------------------------------------+
void CalculatePositionStats()
{
    g_buyProfit = 0;
    g_sellProfit = 0;
    g_buyLots = 0;
    g_sellLots = 0;
    g_totalSwap = 0;

    for (int i = PositionsTotal() - 1; i >= 0; i--)
    {
        ulong ticket = PositionGetTicket(i);
        if (PositionSelectByTicket(ticket))
        {
            // RẤT QUAN TRỌNG: Chỉ xử lý lệnh của cặp tiền hiện tại và của EA này
            if (PositionGetString(POSITION_SYMBOL) == _Symbol)
            {

                ENUM_POSITION_TYPE type = (ENUM_POSITION_TYPE)PositionGetInteger(POSITION_TYPE);
                double profit = PositionGetDouble(POSITION_PROFIT);
                double swap = PositionGetDouble(POSITION_SWAP);
                double vol = PositionGetDouble(POSITION_VOLUME);

                g_totalSwap += swap;

                if (type == POSITION_TYPE_BUY)
                {
                    g_buyProfit += profit;
                    g_buyLots += vol;
                }
                else if (type == POSITION_TYPE_SELL)
                {
                    g_sellProfit += profit;
                    g_sellLots += vol;
                }

                if (vol > LotSize && profit >= 20.0)
                {
                    double sl = PositionGetDouble(POSITION_SL);
                    double openPrice = PositionGetDouble(POSITION_PRICE_OPEN);
                    double newSl = openPrice + (type == POSITION_TYPE_BUY ? 1 : -1) * 50 * _Point;

                    if ((type == POSITION_TYPE_BUY && sl < newSl) || (type == POSITION_TYPE_SELL && (sl > newSl || sl == 0)))
                    {
                        if (!Trade.PositionModify(ticket, newSl, 0))
                        {
                            Print("Error modifying position #", ticket, " - Error: ", Trade.ResultRetcode());
                        }
                    }
                }
            }
        }
    }
}

//+------------------------------------------------------------------+
//| Trade Logic                                                      |
//+------------------------------------------------------------------+
void TradeCom()
{
    if (g_buyLots == 0 && g_sellLots == 0)
    { // Sửa lại logic check lệnh trống
        MqlRates rates[];
        ArraySetAsSeries(rates, true);

        if (CopyRates(_Symbol, PERIOD_M1, 0, 2, rates) <= 0)
        {
            Print("Error copying rates: ", GetLastError());
            return;
        }

        if (checkCandle(rates[1]))
        {
            if (!Trade.Sell(LotSize, _Symbol))
                Print("Error placing Sell: ", Trade.ResultRetcode());
        }
        else
        {
            if (!Trade.Buy(LotSize, _Symbol))
                Print("Error placing Buy: ", Trade.ResultRetcode());
        }
    }
    else
    {
        if (g_buyProfit >= g_sellProfit)
        {
            if (!Trade.Buy(LotSize, _Symbol))
                Print("Error placing Buy: ", Trade.ResultRetcode());
        }
        else
        {
            if (!Trade.Sell(LotSize, _Symbol))
                Print("Error placing Sell: ", Trade.ResultRetcode());
        }
    }
}

//+------------------------------------------------------------------+
//| Hedge Logic                                                      |
//+------------------------------------------------------------------+
void HedgePositions()
{
    double totalProfit = g_buyProfit + g_sellProfit + g_totalSwap;

    if (NormalizeDouble(MathAbs(g_sellLots - g_buyLots), 2) == LotSize)
    {
        ulong ticket = PositionGetTicket(PositionsTotal() - 1);

        if (PositionSelectByTicket(ticket))
        {
            if (PositionGetDouble(POSITION_PROFIT) <= -3.0)
            {
                ExecuteHedge(g_buyLots, g_sellLots);
            }
        }
    }
    else if ((g_sellLots == 0 || g_buyLots == 0))
    {
        if (totalProfit <= -3.0)
        {
            ExecuteHedge(g_buyLots, g_sellLots);
        }
    }
}

//+------------------------------------------------------------------+
//| Đóng toàn bộ lệnh                                                |
//+------------------------------------------------------------------+
void CloseAllPositions()
{
    Trade.SetAsyncMode(true);
    for (int i = PositionsTotal() - 1; i >= 0; i--)
    {
        ulong ticket = PositionGetTicket(i);
        if (PositionSelectByTicket(ticket) && PositionGetString(POSITION_SYMBOL) == _Symbol)
        {
            if (!Trade.PositionClose(ticket))
            {
                Print("Close failed #", ticket, " - Error: ", Trade.ResultRetcode());
            }
        }
    }
    Trade.SetAsyncMode(false);
}

//+------------------------------------------------------------------+
//| Cân bằng Hedge                                                   |
//+------------------------------------------------------------------+
void ExecuteHedge(double bLots, double sLots)
{
    double diff = NormalizeDouble(bLots - sLots, 2);

    if (diff > 0)
    {
        if (!Trade.Sell(MathAbs(diff), _Symbol))
            Print("Error Sell Hedge: ", Trade.ResultRetcode());
    }
    else if (diff < 0)
    {
        if (!Trade.Buy(MathAbs(diff), _Symbol))
            Print("Error Buy Hedge: ", Trade.ResultRetcode());
    }
}

//+------------------------------------------------------------------+
//| Tạo Label UI                                                     |
//+------------------------------------------------------------------+
bool CreateLable(CChartObjectLabel &lable, string name, string des, int x, int y)
{
    // Tạo lable và thiết lập thuộc tính
    if (!lable.Create(0, name, 0, x, y))
        return false;

    lable.Description(des);
    lable.Color(clrWhite);
    lable.Font("Calibri");
    lable.FontSize(12);

    return true;
}

bool CreateText(CChartObjectText &txtObj, string name, string des)
{
    // Khởi tạo ở thời gian 0, giá 0. Sau đó sẽ di chuyển sau.
    if (!txtObj.Create(0, name, 0, 0, 0.0))
        return false;

    txtObj.Description(des);
    txtObj.Color(clrWhite);
    txtObj.Font("Calibri");
    txtObj.FontSize(12);
    txtObj.Anchor(ANCHOR_LEFT); // Canh lề trái để chữ nằm ngay bên phải nến

    return true;
}
//+------------------------------------------------------------------+
//| Phân tích hành vi nến                                            |
//+------------------------------------------------------------------+
bool checkCandle(const MqlRates &rate)
{
    if (rate.close < rate.open)
    { // Nến giảm
        double body = rate.open - rate.close;
        double lowerWick = rate.close - rate.low;
        double upperWick = rate.high - rate.open;
        return lowerWick <= body * 2 || lowerWick * 2 <= upperWick;
    }
    else
    { // Nến tăng
        double body = rate.close - rate.open;
        double upperWick = rate.high - rate.close;
        double lowerWick = rate.open - rate.low;
        return upperWick > body * 2 && upperWick >= lowerWick * 2;
    }
}

//+------------------------------------------------------------------+
//| Lấy tổng Lot đã giao dịch trong ngày                             |
//+------------------------------------------------------------------+
double GetDailyTradedLots()
{
    double totalLots = 0.0;
    datetime startOfDay = iTime(_Symbol, PERIOD_D1, 0);
    datetime currentTime = TimeCurrent();

    if (HistorySelect(startOfDay, currentTime))
    {
        int dealsTotal = HistoryDealsTotal();
        for (int i = 0; i < dealsTotal; i++)
        {
            ulong dealTicket = HistoryDealGetTicket(i);
            if (dealTicket > 0 && HistoryDealGetString(dealTicket, DEAL_SYMBOL) == _Symbol)
            {
                if (HistoryDealGetInteger(dealTicket, DEAL_ENTRY) == DEAL_ENTRY_IN)
                {
                    totalLots += HistoryDealGetDouble(dealTicket, DEAL_VOLUME);
                }
            }
        }
    }
    return NormalizeDouble(totalLots, 2);
}