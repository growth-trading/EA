#include <Trade\Trade.mqh>
#include <ChartObjects\ChartObjectsTxtControls.mqh>

CTrade Trade;
CChartObjectLabel lblDailyProfit, lblDailyPct, lblLocalTime, lblServerTime, lblStatus;
CChartObjectLabel lblCondHours, lblCondDaily, lblCondPos;
CChartObjectText  txtTimeCountDown;

input group  "=== Cài đặt lệnh ==="
input double LotSize         = 0.01;      // Khối lượng mỗi lệnh (lot)
input int    MaxPositions    = 20;        // Số lệnh tối đa đồng thời
input int    MagicNumber     = 20250526;  // Magic number (phân biệt với EA khác)

input group  "=== Lợi nhuận & Rủi ro ==="
input double TakeProfitUSD   = 1.5;       // Chốt lời chu kỳ (USD)
input double MaxDrawdownUSD  = 50.0;      // Cắt lỗ tối đa chu kỳ (USD)
input double DailyProfitPct  = 2.0;       // Mục tiêu lợi nhuận ngày - dừng bot (% số dư đầu ngày)
input double DailyLossPct    = 20.0;      // Lỗ tối đa trong ngày - dừng bot (% số dư đầu ngày)
input int    TrailTriggerPips = 30;        // Số pip lãi để kích hoạt trailing SL
input int    TrailPips       = 50;        // Khoảng cách trailing SL tính từ giá mở (pip)

input group  "=== Thời gian giao dịch ==="
input int    LocalUTCOffset  = 7;         // Múi giờ địa phương (VD: 7 = UTC+7)
input int    StartHour       = 8;         // Giờ bắt đầu giao dịch (giờ địa phương)
input int    EndHour         = 22;        // Giờ kết thúc giao dịch (giờ địa phương)

input group  "=== Giao diện panel ==="
input int    PanelRight      = 220;       // Khoảng cách từ mép phải màn hình (pixel)
input int    PanelTop        = 25;        // Vị trí từ trên xuống (pixel)

// Global variables
datetime lastCandleTime    = 0;
bool     g_dailyTargetHit  = false;
bool     g_dailyLossHit    = false;
datetime g_currentDay      = 0;
double   g_dayStartBalance = 0.0;

// Variables for one-time tick calculation
double g_buyProfit       = 0.0;
double g_sellProfit      = 0.0;
double g_buyLots         = 0.0;
double g_sellLots        = 0.0;
double g_totalSwap       = 0.0;
int    g_totalPos        = 0;
double g_dailyClosedPnL  = 0.0; // Lợi nhuận đã đóng trong ngày (cache từ OnTimer)

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
{
    Trade.SetExpertMagicNumber(MagicNumber);
    EventSetTimer(1);

    g_currentDay      = iTime(_Symbol, PERIOD_D1, 0);
    g_dayStartBalance = AccountInfoDouble(ACCOUNT_BALANCE);

    int x = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS) - PanelRight;
    int y = PanelTop;

    if (!CreateLable(lblDailyProfit, "DailyProfit", "Daily P&L : --",              x, y     )) return INIT_FAILED;
    if (!CreateLable(lblDailyPct,    "DailyPct",   "Now --% / MaxLoss --% / Profit --%", x, y+ 22 )) return INIT_FAILED;
    if (!CreateLable(lblLocalTime,   "LocalTime",   "Local  : --:--:--",           x, y+ 44 )) return INIT_FAILED;
    if (!CreateLable(lblServerTime,  "ServerTime",  "Server : --:--:--",           x, y+ 66 )) return INIT_FAILED;
    if (!CreateLable(lblStatus,      "BotStatus",   "Status : --",                 x, y+ 88 )) return INIT_FAILED;
    if (!CreateLable(lblCondHours,   "CondHours",   "Hours  : --",                 x, y+115 )) return INIT_FAILED;
    if (!CreateLable(lblCondDaily,   "CondDaily",   "Daily  : --",                 x, y+137 )) return INIT_FAILED;
    if (!CreateLable(lblCondPos,     "CondPos",     "Pos    : --",                 x, y+159 )) return INIT_FAILED;

    if (!CreateText(txtTimeCountDown, "TimeCountDown", "Countdown:  s"))    return INIT_FAILED;

    ChartRedraw(0);
    return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                 |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
    EventKillTimer();
}

//+------------------------------------------------------------------+
//| UI Update function (mỗi giây)                                    |
//+------------------------------------------------------------------+
void OnTimer()
{
    // --- Kiểm tra ngày mới → reset trạng thái hàng ngày ---
    datetime today = iTime(_Symbol, PERIOD_D1, 0);
    if (today != g_currentDay)
    {
        g_currentDay      = today;
        g_dailyTargetHit  = false;
        g_dailyLossHit    = false;
        g_dayStartBalance = AccountInfoDouble(ACCOUNT_BALANCE);
        Print("Ngày mới - balance đầu ngày: ", DoubleToString(g_dayStartBalance, 2), " USD");
    }

    // --- Cập nhật lợi nhuận đã đóng trong ngày (cache mỗi giây) ---
    g_dailyClosedPnL = GetDailyClosedProfit();

    // --- Daily P&L ---
    double dailyTotal        = g_dailyClosedPnL + g_buyProfit + g_sellProfit + g_totalSwap;
    double dailyProfitTarget = g_dayStartBalance * DailyProfitPct / 100.0;
    double dailyPct          = (g_dayStartBalance > 0) ? dailyTotal / g_dayStartBalance * 100.0 : 0.0;
    string pnlSign           = dailyTotal >= 0 ? "+" : "";
    string pctSign           = dailyPct  >= 0 ? "+" : "";
    lblDailyProfit.Description("Daily P&L : " + pnlSign + DoubleToString(dailyTotal, 2) + " USD");
    lblDailyProfit.Color(dailyTotal >= 0 ? clrLimeGreen : clrTomato);
    lblDailyPct.Description("Now " + pctSign + DoubleToString(dailyPct, 1)
                            + "% / MaxLoss -" + DoubleToString(DailyLossPct, 1)
                            + "% / Profit +" + DoubleToString(DailyProfitPct, 1) + "%");
    lblDailyPct.Color(dailyTotal >= 0 ? clrLimeGreen : clrTomato);

    // --- Giờ địa phương (theo UTC offset input) và giờ server ---
    MqlDateTime localDt, serverDt;
    TimeToStruct(TimeGMT() + LocalUTCOffset * 3600, localDt);
    TimeToStruct(TimeCurrent(),                     serverDt);
    lblLocalTime.Description(StringFormat("Local  : %02d:%02d:%02d  (UTC+%d)", localDt.hour, localDt.min, localDt.sec, LocalUTCOffset));
    lblServerTime.Description(StringFormat("Server : %02d:%02d:%02d", serverDt.hour, serverDt.min, serverDt.sec));

    // --- Trạng thái bot ---
    if (g_dailyTargetHit)
    {
        lblStatus.Description("Status: Đạt mục tiêu ngày");
        lblStatus.Color(clrGold);
    }
    else if (g_dailyLossHit)
    {
        lblStatus.Description("Status: Đạt lỗ tối đa ngày");
        lblStatus.Color(clrTomato);
    }
    else if (!IsInTradingHours())
    {
        lblStatus.Description("Status: Ngoài giờ giao dịch");
        lblStatus.Color(clrSilver);
    }
    else
    {
        lblStatus.Description("Status: Đang hoạt động");
        lblStatus.Color(clrLimeGreen);
    }

    // --- Điều kiện mở lệnh ---
    bool condHours  = IsInTradingHours();
    bool condDaily  = !g_dailyTargetHit && !g_dailyLossHit;
    bool condMaxPos = (g_totalPos < MaxPositions);

    if (condHours)
    { lblCondHours.Description(StringFormat("Hours  : OK (%d:00 - %d:00)", StartHour, EndHour)); lblCondHours.Color(clrLimeGreen); }
    else
    { lblCondHours.Description("Hours  : Ngoài khung giờ giao dịch");                            lblCondHours.Color(clrSilver); }

    if (condDaily)
    { lblCondDaily.Description("Daily  : OK - Chưa đạt target");          lblCondDaily.Color(clrWhite); }
    else if (g_dailyLossHit)
    { lblCondDaily.Description("Daily  : Đạt lỗ tối đa - Bot dừng");     lblCondDaily.Color(clrTomato); }
    else
    { lblCondDaily.Description("Daily  : Đạt target - Bot dừng");         lblCondDaily.Color(clrGold); }

    if (condMaxPos)
    { lblCondPos.Description("Pos    : " + IntegerToString(g_totalPos) + " / " + IntegerToString(MaxPositions)); lblCondPos.Color(clrWhite); }
    else
    { lblCondPos.Description("Pos    : Đã đủ max (" + IntegerToString(MaxPositions) + "/" + IntegerToString(MaxPositions) + ")"); lblCondPos.Color(clrOrange); }

    // --- Countdown đến nến tiếp theo ---
    datetime currentTime    = TimeCurrent();
    datetime nextCandleTime = iTime(_Symbol, PERIOD_CURRENT, 0) + PeriodSeconds(PERIOD_CURRENT);
    int secondsToNextCandle = MathMax(0, (int)(nextCandleTime - currentTime - 1));

    txtTimeCountDown.Description("    " + IntegerToString(secondsToNextCandle) + "s");
    txtTimeCountDown.Time(0, nextCandleTime);
    txtTimeCountDown.Price(0, iClose(_Symbol, PERIOD_CURRENT, 0));

    ChartRedraw();
}

//+------------------------------------------------------------------+
//| Expert tick function                                             |
//+------------------------------------------------------------------+
void OnTick()
{
    CalculatePositionStats();

    double totalNetProfit = g_buyProfit + g_sellProfit + g_totalSwap;

    // --- Kiểm tra daily profit target ---
    double dailyTotal       = g_dailyClosedPnL + totalNetProfit;
    double dailyProfitTarget = g_dayStartBalance * DailyProfitPct / 100.0;
    if (!g_dailyTargetHit && dailyTotal >= dailyProfitTarget)
    {
        CloseAllPositions();
        g_dailyTargetHit = true;
        Print("Daily profit target đạt: ", DoubleToString(dailyTotal, 2), " / ",
              DoubleToString(dailyProfitTarget, 2), " USD (", DoubleToString(DailyProfitPct, 1), "%) - Dừng hôm nay.");
        return;
    }

    // --- Kiểm tra TP/SL chu kỳ ---
    if (totalNetProfit >= TakeProfitUSD || totalNetProfit <= -MaxDrawdownUSD)
    {
        CloseAllPositions();
        return;
    }

    // --- Kiểm tra daily loss limit ---
    double dailyLossLimit = g_dayStartBalance * DailyLossPct / 100.0;
    if (!g_dailyLossHit && dailyTotal <= -dailyLossLimit)
    {
        CloseAllPositions();
        g_dailyLossHit = true;
        Print("Daily loss limit đạt: ", DoubleToString(dailyTotal, 2), " / -",
              DoubleToString(dailyLossLimit, 2), " USD (", DoubleToString(DailyLossPct, 1), "%) - Dừng hôm nay.");
        return;
    }

    // --- Mở lệnh theo nến M1 ---
    datetime currentCandleTime = iTime(_Symbol, PERIOD_M1, 0);
    if (currentCandleTime != lastCandleTime)
    {
        lastCandleTime = currentCandleTime;
        TradeCom();
    }

    HedgePositions();
}

//+------------------------------------------------------------------+
//| Kiểm tra trong khung giờ giao dịch (so sánh theo giờ địa phương) |
//+------------------------------------------------------------------+
bool IsInTradingHours()
{
    MqlDateTime dt;
    datetime localTime = TimeGMT() + LocalUTCOffset * 3600;
    TimeToStruct(localTime, dt);
    return dt.hour >= StartHour && dt.hour < EndHour;
}

//+------------------------------------------------------------------+
//| Core Function: Duyệt lệnh 1 lần để lấy toàn bộ thông tin         |
//+------------------------------------------------------------------+
void CalculatePositionStats()
{
    g_buyProfit  = 0;
    g_sellProfit = 0;
    g_buyLots    = 0;
    g_sellLots   = 0;
    g_totalSwap  = 0;
    g_totalPos   = 0;

    for (int i = PositionsTotal() - 1; i >= 0; i--)
    {
        ulong ticket = PositionGetTicket(i);
        if (!PositionSelectByTicket(ticket))
            continue;
        if (PositionGetString(POSITION_SYMBOL) != _Symbol)
            continue;
        if ((int)PositionGetInteger(POSITION_MAGIC) != MagicNumber)
            continue;

        ENUM_POSITION_TYPE type   = (ENUM_POSITION_TYPE)PositionGetInteger(POSITION_TYPE);
        double             profit = PositionGetDouble(POSITION_PROFIT);
        double             swap   = PositionGetDouble(POSITION_SWAP);
        double             vol    = PositionGetDouble(POSITION_VOLUME);

        g_totalSwap += swap;
        g_totalPos++;

        if (type == POSITION_TYPE_BUY)
        {
            g_buyProfit += profit;
            g_buyLots   += vol;
        }
        else
        {
            g_sellProfit += profit;
            g_sellLots   += vol;
        }

        if (vol > LotSize)
        {
            double sl           = PositionGetDouble(POSITION_SL);
            double openPrice    = PositionGetDouble(POSITION_PRICE_OPEN);
            double currentPrice = PositionGetDouble(POSITION_PRICE_CURRENT);
            double pipsMoved    = (type == POSITION_TYPE_BUY)
                                  ? (currentPrice - openPrice) / _Point
                                  : (openPrice - currentPrice) / _Point;

            if (pipsMoved < TrailTriggerPips)
                continue;

            double newSl = openPrice + (type == POSITION_TYPE_BUY ? 1 : -1) * TrailPips * _Point;

            if ((type == POSITION_TYPE_BUY && sl < newSl) ||
                (type == POSITION_TYPE_SELL && (sl > newSl || sl == 0)))
            {
                if (!Trade.PositionModify(ticket, newSl, 0))
                    Print("Error modifying position #", ticket, " - Error: ", Trade.ResultRetcode());
            }
        }
    }
}

//+------------------------------------------------------------------+
//| Trade Logic                                                      |
//+------------------------------------------------------------------+
void TradeCom()
{
    if (g_dailyTargetHit || g_dailyLossHit) return;
    if (!IsInTradingHours()) return;
    if (g_totalPos >= MaxPositions) return;

    if (g_buyLots < 0.001 && g_sellLots < 0.001)
    {
        MqlRates rates[];
        ArraySetAsSeries(rates, true);

        if (CopyRates(_Symbol, PERIOD_M1, 0, 2, rates) <= 0)
        {
            Print("Error copying rates: ", GetLastError());
            return;
        }

        bool sellSignal = isShootingStar(rates[1]);
        bool buySignal  = isHammer(rates[1]);

        // Có pattern rõ → ưu tiên; không có → dùng chiều đóng cửa nến
        bool doSell = sellSignal || (!buySignal && rates[1].close < rates[1].open);

        if (doSell)
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
    if (g_totalPos == 0)
        return;

    double totalProfit = g_buyProfit + g_sellProfit + g_totalSwap;

    if (NormalizeDouble(MathAbs(g_sellLots - g_buyLots), 2) == LotSize)
    {
        ulong ticket = GetLatestPositionTicket();

        if (ticket > 0 && PositionSelectByTicket(ticket))
        {
            if (PositionGetDouble(POSITION_PROFIT) <= -3.0)
                ExecuteHedge(g_buyLots, g_sellLots);
        }
    }
    else if (g_sellLots < 0.001 || g_buyLots < 0.001)
    {
        if (totalProfit <= -3.0)
            ExecuteHedge(g_buyLots, g_sellLots);
    }
}

//+------------------------------------------------------------------+
//| Tìm ticket của lệnh mở gần nhất thuộc EA này                     |
//+------------------------------------------------------------------+
ulong GetLatestPositionTicket()
{
    ulong    latestTicket = 0;
    datetime latestTime   = 0;

    for (int i = PositionsTotal() - 1; i >= 0; i--)
    {
        ulong ticket = PositionGetTicket(i);
        if (!PositionSelectByTicket(ticket))
            continue;
        if (PositionGetString(POSITION_SYMBOL) != _Symbol)
            continue;
        if ((int)PositionGetInteger(POSITION_MAGIC) != MagicNumber)
            continue;

        datetime openTime = (datetime)PositionGetInteger(POSITION_TIME);
        if (openTime > latestTime)
        {
            latestTime   = openTime;
            latestTicket = ticket;
        }
    }
    return latestTicket;
}

//+------------------------------------------------------------------+
//| Đóng toàn bộ lệnh của EA này                                     |
//+------------------------------------------------------------------+
void CloseAllPositions()
{
    Trade.SetAsyncMode(true);
    for (int i = PositionsTotal() - 1; i >= 0; i--)
    {
        ulong ticket = PositionGetTicket(i);
        if (!PositionSelectByTicket(ticket))
            continue;
        if (PositionGetString(POSITION_SYMBOL) != _Symbol)
            continue;
        if ((int)PositionGetInteger(POSITION_MAGIC) != MagicNumber)
            continue;

        if (!Trade.PositionClose(ticket))
            Print("Close failed #", ticket, " - Error: ", Trade.ResultRetcode());
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
    if (!txtObj.Create(0, name, 0, 0, 0.0))
        return false;

    txtObj.Description(des);
    txtObj.Color(clrWhite);
    txtObj.Font("Calibri");
    txtObj.FontSize(12);
    txtObj.Anchor(ANCHOR_LEFT);

    return true;
}

//+------------------------------------------------------------------+
//| Shooting Star: bóng trên dài, báo đảo chiều xuống → Sell        |
//+------------------------------------------------------------------+
bool isShootingStar(const MqlRates &rate)
{
    double body, upperWick, lowerWick;

    if (rate.close < rate.open)
    {
        body      = rate.open - rate.close;
        lowerWick = rate.close - rate.low;
        upperWick = rate.high - rate.open;
    }
    else
    {
        body      = rate.close - rate.open;
        upperWick = rate.high - rate.close;
        lowerWick = rate.open - rate.low;
    }

    if (body == 0) return false;
    return upperWick > body * 2 && upperWick >= lowerWick * 2;
}

//+------------------------------------------------------------------+
//| Hammer: bóng dưới dài, báo đảo chiều lên → Buy                  |
//+------------------------------------------------------------------+
bool isHammer(const MqlRates &rate)
{
    double body, upperWick, lowerWick;

    if (rate.close < rate.open)
    {
        body      = rate.open - rate.close;
        lowerWick = rate.close - rate.low;
        upperWick = rate.high - rate.open;
    }
    else
    {
        body      = rate.close - rate.open;
        upperWick = rate.high - rate.close;
        lowerWick = rate.open - rate.low;
    }

    if (body == 0) return false;
    return lowerWick > body * 2 && lowerWick >= upperWick * 2;
}

//+------------------------------------------------------------------+
//| Lợi nhuận từ các lệnh đã đóng trong ngày hôm nay                 |
//+------------------------------------------------------------------+
double GetDailyClosedProfit()
{
    double   totalProfit = 0.0;
    datetime startOfDay  = iTime(_Symbol, PERIOD_D1, 0);
    datetime currentTime = TimeCurrent();

    if (!HistorySelect(startOfDay, currentTime))
        return 0.0;

    int dealsTotal = HistoryDealsTotal();
    for (int i = 0; i < dealsTotal; i++)
    {
        ulong dealTicket = HistoryDealGetTicket(i);
        if (dealTicket == 0)
            continue;
        if (HistoryDealGetString(dealTicket, DEAL_SYMBOL) != _Symbol)
            continue;
        if ((int)HistoryDealGetInteger(dealTicket, DEAL_MAGIC) != MagicNumber)
            continue;
        if (HistoryDealGetInteger(dealTicket, DEAL_ENTRY) != DEAL_ENTRY_OUT)
            continue;

        totalProfit += HistoryDealGetDouble(dealTicket, DEAL_PROFIT);
        totalProfit += HistoryDealGetDouble(dealTicket, DEAL_SWAP);
        totalProfit += HistoryDealGetDouble(dealTicket, DEAL_COMMISSION);
    }
    return totalProfit;
}

//+------------------------------------------------------------------+
//| Lấy tổng Lot đã giao dịch trong ngày (chỉ tính lệnh của EA này)  |
//+------------------------------------------------------------------+
double GetDailyTradedLots()
{
    double   totalLots   = 0.0;
    datetime startOfDay  = iTime(_Symbol, PERIOD_D1, 0);
    datetime currentTime = TimeCurrent();

    if (!HistorySelect(startOfDay, currentTime))
        return 0.0;

    int dealsTotal = HistoryDealsTotal();
    for (int i = 0; i < dealsTotal; i++)
    {
        ulong dealTicket = HistoryDealGetTicket(i);
        if (dealTicket > 0 &&
            HistoryDealGetString(dealTicket, DEAL_SYMBOL) == _Symbol &&
            (int)HistoryDealGetInteger(dealTicket, DEAL_MAGIC) == MagicNumber)
        {
            if (HistoryDealGetInteger(dealTicket, DEAL_ENTRY) == DEAL_ENTRY_IN)
                totalLots += HistoryDealGetDouble(dealTicket, DEAL_VOLUME);
        }
    }
    return NormalizeDouble(totalLots, 2);
}
