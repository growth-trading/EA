#include <Trade\Trade.mqh>
#include <ChartObjects\ChartObjectsTxtControls.mqh>

CTrade Trade;
CChartObjectRectLabel panelBg, sep1, sep2, sep3, sep4;
CChartObjectLabel lblTime,    lblStatus,  lblHours;
CChartObjectLabel lblBalance, lblInitial, lblDayPnL,  lblFloat;
CChartObjectLabel lblDDNow,   lblDDMax,   lblProfitDay;
CChartObjectLabel lblBuyPnL,  lblBuyOrd,  lblSelPnL,  lblSelOrd, lblTotal;
CChartObjectLabel lblADX;
CChartObjectText   txtTimeCountDown;
CChartObjectButton btnReset;

input group  "=== Cài đặt lệnh ==="
input double LotSize         = 0.01;      // Khối lượng mỗi lệnh (lot)
input int    MaxPositions    = 50;        // Số lệnh tối đa đồng thời
input int    MagicNumber     = 20250526;  // Magic number (phân biệt với EA khác)

input group  "=== Lợi nhuận & Rủi ro ==="
input double TakeProfitUSD   = 1.5;       // Chốt lời chu kỳ (USD)
input double CycleLossUSD    = 25.0;      // Cắt lỗ chu kỳ - đóng loạt lệnh (USD)
input double DailyProfitUSD  = 10.0;      // Mục tiêu lợi nhuận ngày - dừng bot (USD)
input double DailyLossUSD    = 50.0;      // Lỗ tối đa trong ngày - dừng bot (USD)
input int    TrailTriggerPips = 30;        // Số pip lãi để kích hoạt trailing SL
input int    TrailPips       = 50;        // Khoảng cách trailing SL tính từ giá mở (pip)

input group  "=== Thời gian giao dịch ==="
input int    LocalUTCOffset  = 7;         // Múi giờ địa phương (VD: 7 = UTC+7)
input int    StartHour       = 7;         // Giờ bắt đầu giao dịch (giờ địa phương)
input int    StartMinute     = 0;         // Phút bắt đầu giao dịch
input int    EndHour         = 24;        // Giờ kết thúc giao dịch (giờ địa phương)
input int    EndMinute       = 0;         // Phút kết thúc giao dịch

input group  "=== Bộ lọc ADX ==="
input int             ADXPeriod      = 7;          // Chu kỳ ADX
input ENUM_TIMEFRAMES ADXTimeframe   = PERIOD_M1;   // Khung thời gian ADX
input double          ADXMinStrength = 20.0;        // Ngưỡng ADX tối thiểu để vào lệnh

input group  "=== Giao diện panel ==="
input int    PanelLeft       = 5;         // Khoảng cách từ mép trái màn hình (pixel)
input int    PanelTop        = 25;        // Vị trí từ trên xuống (pixel)
input int    PanelWidth      = 290;       // Chiều rộng panel (pixel)

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
int    g_buyCount        = 0;
int    g_sellCount       = 0;
double g_dailyClosedPnL  = 0.0; // Lợi nhuận đã đóng trong ngày (cache từ OnTimer)
double g_closedPnLOffset = 0.0; // Offset khi reset giữa ngày
int    g_adxHandle     = INVALID_HANDLE;
double g_adxValue      = 0.0;

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit()
{
    Trade.SetExpertMagicNumber(MagicNumber);
    EventSetTimer(1);

    g_currentDay      = iTime(_Symbol, PERIOD_D1, 0);
    g_dayStartBalance = AccountInfoDouble(ACCOUNT_BALANCE);

    int x = PanelLeft + 10;
    int y = PanelTop;
    int sw = PanelWidth - 20;  // separator width

    // --- Nền panel ---
    g_adxHandle = iADX(_Symbol, ADXTimeframe, ADXPeriod);
    if (g_adxHandle == INVALID_HANDLE) return INIT_FAILED;

    if (!panelBg.Create(0, "cbPanelBg", 0, PanelLeft, y - 10, PanelWidth, 427)) return INIT_FAILED;
    panelBg.BackColor(C'15,20,28');
    panelBg.BorderType(BORDER_FLAT);
    panelBg.Color(C'50,65,80');

    // Nhóm 1: Thời gian & trạng thái
    if (!CreateLable(lblTime,    "cbTime",    "Time  : --",             x, y     )) return INIT_FAILED;
    if (!CreateLable(lblStatus,  "cbStatus",  "Status: --",             x, y+ 22 )) return INIT_FAILED;
    if (!CreateLable(lblHours,   "cbHours",   "Hours : --",             x, y+ 44 )) return INIT_FAILED;
    if (!CreateLable(lblADX,     "cbADX",     "ADX   : --",             x, y+ 66 )) return INIT_FAILED;

    // Separator 1
    if (!sep1.Create(0, "cbSep1", 0, PanelLeft+5, y+82, sw, 1)) return INIT_FAILED;
    sep1.BackColor(C'50,65,80'); sep1.BorderType(BORDER_FLAT); sep1.Color(C'50,65,80');

    // Nhóm 2: Tài khoản
    if (!CreateLable(lblBalance, "cbBalance", "Balance: $--",           x, y+ 90 )) return INIT_FAILED;
    if (!CreateLable(lblInitial, "cbInitial", "Initial: $--",           x, y+112 )) return INIT_FAILED;
    if (!CreateLable(lblDayPnL,  "cbDayPnL",  "Day P/L: $--",          x, y+134 )) return INIT_FAILED;
    if (!CreateLable(lblFloat,   "cbFloat",   "Float  : $--",           x, y+156 )) return INIT_FAILED;

    // Separator 2
    if (!sep2.Create(0, "cbSep2", 0, PanelLeft+5, y+174, sw, 1)) return INIT_FAILED;
    sep2.BackColor(C'50,65,80'); sep2.BorderType(BORDER_FLAT); sep2.Color(C'50,65,80');

    // Nhóm 3: DD & Profit
    if (!CreateLable(lblDDNow,     "cbDDNow",     "DD Now    : --%",     x, y+182 )) return INIT_FAILED;
    if (!CreateLable(lblDDMax,     "cbDDMax",     "DD Max    : --%",     x, y+204 )) return INIT_FAILED;
    if (!CreateLable(lblProfitDay, "cbProfitDay", "Profit Day: $--",     x, y+226 )) return INIT_FAILED;

    // Separator 3
    if (!sep3.Create(0, "cbSep3", 0, PanelLeft+5, y+244, sw, 1)) return INIT_FAILED;
    sep3.BackColor(C'50,65,80'); sep3.BorderType(BORDER_FLAT); sep3.Color(C'50,65,80');

    // Nhóm 4: Buy / Sell
    if (!CreateLable(lblBuyPnL,  "cbBuyPnL",  "Buy P/L: $--",            x, y+252 )) return INIT_FAILED;
    if (!CreateLable(lblBuyOrd,  "cbBuyOrd",  "Buy Ord: 0    Lot: 0.00", x, y+274 )) return INIT_FAILED;
    if (!CreateLable(lblSelPnL,  "cbSelPnL",  "Sel P/L: $--",            x, y+296 )) return INIT_FAILED;
    if (!CreateLable(lblSelOrd,  "cbSelOrd",  "Sel Ord: 0    Lot: 0.00", x, y+318 )) return INIT_FAILED;
    if (!CreateLable(lblTotal,   "cbTotal",   "Total  : 0 orders",        x, y+340 )) return INIT_FAILED;

    // Separator 4
    if (!sep4.Create(0, "cbSep4", 0, PanelLeft+5, y+362, sw, 1)) return INIT_FAILED;
    sep4.BackColor(C'50,65,80'); sep4.BorderType(BORDER_FLAT); sep4.Color(C'50,65,80');

    // Nút reset
    if (!btnReset.Create(0, "cbBtnReset", 0, x, y+370, sw, 28)) return INIT_FAILED;
    btnReset.Description("Reset Daily");
    btnReset.Color(clrWhite);
    btnReset.BackColor(C'150,45,35');
    btnReset.Font("Calibri");
    btnReset.FontSize(12);

    if (!CreateText(txtTimeCountDown, "TimeCountDown", "Countdown:  s"))    return INIT_FAILED;

    ChartRedraw(0);
    return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| Expert deinitialization function                                 |
//+------------------------------------------------------------------+
void OnDeinit(const int reason)
{
    if (g_adxHandle != INVALID_HANDLE) IndicatorRelease(g_adxHandle);
    EventKillTimer();
}

//+------------------------------------------------------------------+
//| Reset thống kê hàng ngày                                         |
//+------------------------------------------------------------------+
void ResetDailyStats()
{
    g_dailyTargetHit  = false;
    g_dailyLossHit    = false;
    g_dayStartBalance = AccountInfoDouble(ACCOUNT_BALANCE);
    g_currentDay      = iTime(_Symbol, PERIOD_D1, 0);
    g_closedPnLOffset = GetDailyClosedProfit();
    g_dailyClosedPnL  = 0.0;
    g_buyProfit       = 0.0;
    g_sellProfit      = 0.0;
    g_buyLots         = 0.0;
    g_sellLots        = 0.0;
    g_totalSwap       = 0.0;
    g_totalPos        = 0;
    g_buyCount        = 0;
    g_sellCount       = 0;
    Print("Manual reset - balance: ", DoubleToString(g_dayStartBalance, 2), " USD");
}

//+------------------------------------------------------------------+
//| Chart event handler                                              |
//+------------------------------------------------------------------+
void OnChartEvent(const int id, const long &lparam, const double &dparam, const string &sparam)
{
    if (id == CHARTEVENT_OBJECT_CLICK && sparam == "cbBtnReset")
    {
        ResetDailyStats();
        btnReset.State(false);
    }
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
        g_closedPnLOffset = 0.0;
        Print("Ngày mới - balance đầu ngày: ", DoubleToString(g_dayStartBalance, 2), " USD");
    }

    // --- Cập nhật lợi nhuận đã đóng trong ngày (cache mỗi giây) ---
    g_dailyClosedPnL = GetDailyClosedProfit() - g_closedPnLOffset;

    double floatPnL   = g_buyProfit + g_sellProfit + g_totalSwap;
    double dailyTotal = g_dailyClosedPnL + floatPnL;

    // --- Time ---
    MqlDateTime serverDt;
    TimeToStruct(TimeCurrent(), serverDt);
    lblTime.Description(StringFormat("Time  : %04d/%02d/%02d  %02d:%02d:%02d",
                        serverDt.year, serverDt.mon, serverDt.day,
                        serverDt.hour, serverDt.min, serverDt.sec));

    // --- Status ---
    if (g_dailyTargetHit)
    { lblStatus.Description("Status: Đạt mục tiêu ngày");  lblStatus.Color(clrGold); }
    else if (g_dailyLossHit)
    { lblStatus.Description("Status: Đạt lỗ tối đa ngày"); lblStatus.Color(clrTomato); }
    else if (!IsInTradingHours())
    {
        if (g_totalPos > 0)
        { lblStatus.Description("Status: Ngoài giờ - xử lý " + IntegerToString(g_totalPos) + " lệnh"); lblStatus.Color(clrOrange); }
        else
        { lblStatus.Description("Status: Ngoài giờ giao dịch"); lblStatus.Color(clrSilver); }
    }
    else
    { lblStatus.Description("Status: Đang hoạt động"); lblStatus.Color(clrLimeGreen); }

    // --- Hours ---
    if (IsInTradingHours())
    { lblHours.Description(StringFormat("Hours : %02d:%02d - %02d:%02d  OK", StartHour, StartMinute, EndHour, EndMinute)); lblHours.Color(clrLimeGreen); }
    else
    { lblHours.Description(StringFormat("Hours : %02d:%02d - %02d:%02d  --", StartHour, StartMinute, EndHour, EndMinute)); lblHours.Color(clrSilver); }

    // --- ADX ---
    bool adxOK = g_adxValue >= ADXMinStrength;
    lblADX.Description(StringFormat("ADX   : %.1f  (min %.0f)  %s", g_adxValue, ADXMinStrength, adxOK ? "OK" : "--"));
    lblADX.Color(adxOK ? clrLimeGreen : clrSilver);

    // --- Balance & Initial ---
    double balance = AccountInfoDouble(ACCOUNT_BALANCE);
    lblBalance.Description("Balance: $" + DoubleToString(balance, 2));
    lblBalance.Color(clrWhite);
    lblInitial.Description("Initial: $" + DoubleToString(g_dayStartBalance, 2));
    lblInitial.Color(clrSilver);

    // --- Day P/L (chỉ lệnh đã đóng) ---
    string daySign = g_dailyClosedPnL >= 0 ? "+" : "";
    lblDayPnL.Description("Day P/L: $" + daySign + DoubleToString(g_dailyClosedPnL, 2));
    lblDayPnL.Color(g_dailyClosedPnL >= 0 ? clrLimeGreen : clrTomato);

    // --- Float (lệnh đang mở) ---
    double floatPct  = (g_dayStartBalance > 0) ? floatPnL / g_dayStartBalance * 100.0 : 0.0;
    string floatSign = floatPnL >= 0 ? "+" : "";
    lblFloat.Description(StringFormat("Float  : $%.2f  (%.2f%%)", floatPnL, floatPct));
    lblFloat.Color(floatPnL >= 0 ? clrLimeGreen : clrTomato);

    // --- DD Now / DD Max ---
    double ddNow = (floatPnL < 0 && g_dayStartBalance > 0) ? MathAbs(floatPnL) / g_dayStartBalance * 100.0 : 0.0;
    lblDDNow.Description(StringFormat("DD Now : %.2f%%", ddNow));
    lblDDNow.Color(ddNow > 0 ? clrTomato : clrWhite);
    lblDDMax.Description(StringFormat("DD Max : $%.2f", DailyLossUSD));
    lblDDMax.Color(clrSilver);

    // --- Profit Day (closed + open tổng ngày vs mục tiêu) ---
    lblProfitDay.Description(StringFormat("Profit Day: $%.2f  / $%.2f", dailyTotal, DailyProfitUSD));
    lblProfitDay.Color(dailyTotal >= 0 ? clrLimeGreen : clrTomato);

    // --- Buy / Sel ---
    string buyPnLSign = g_buyProfit >= 0 ? "+" : "";
    lblBuyPnL.Description("Buy P/L: $" + buyPnLSign + DoubleToString(g_buyProfit, 2));
    lblBuyPnL.Color(g_buyProfit >= 0 ? clrLimeGreen : clrTomato);
    lblBuyOrd.Description(StringFormat("Buy Ord: %d    Lot: %.2f", g_buyCount, g_buyLots));
    lblBuyOrd.Color(clrWhite);

    string selPnLSign = g_sellProfit >= 0 ? "+" : "";
    lblSelPnL.Description("Sel P/L: $" + selPnLSign + DoubleToString(g_sellProfit, 2));
    lblSelPnL.Color(g_sellProfit >= 0 ? clrLimeGreen : clrTomato);
    lblSelOrd.Description(StringFormat("Sel Ord: %d    Lot: %.2f", g_sellCount, g_sellLots));
    lblSelOrd.Color(clrWhite);

    lblTotal.Description("Total  : " + IntegerToString(g_totalPos) + " / " + IntegerToString(MaxPositions) + " orders");
    lblTotal.Color(g_totalPos >= MaxPositions ? clrOrange : clrWhite);

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
void UpdateADX()
{
    if (g_adxHandle == INVALID_HANDLE) return;
    double buf[];
    if (CopyBuffer(g_adxHandle, 0, 0, 1, buf) > 0)
        g_adxValue = buf[0];
}

void OnTick()
{
    CalculatePositionStats();
    UpdateADX();

    double totalNetProfit = g_buyProfit + g_sellProfit + g_totalSwap;

    // --- Kiểm tra daily profit target ---
    double dailyTotal       = g_dailyClosedPnL + totalNetProfit;
    double dailyProfitTarget = DailyProfitUSD;
    if (!g_dailyTargetHit && dailyTotal >= dailyProfitTarget)
    {
        CloseAllPositions();
        g_dailyTargetHit = true;
        Print("Daily profit target đạt: ", DoubleToString(dailyTotal, 2), " / ",
              DoubleToString(DailyProfitUSD, 2), " USD - Dừng hôm nay.");
        return;
    }

    // --- Kiểm tra TP chu kỳ ---
    if (totalNetProfit >= TakeProfitUSD)
    {
        CloseAllPositions();
        return;
    }

    // --- Kiểm tra SL chu kỳ ---
    if (totalNetProfit <= -CycleLossUSD)
    {
        CloseAllPositions();
        Print("Cycle loss limit đạt: ", DoubleToString(totalNetProfit, 2), " / -",
              DoubleToString(CycleLossUSD, 2), " USD - Đóng loạt lệnh.");
        return;
    }

    // --- Kiểm tra daily loss limit ---
    double dailyLossLimit = DailyLossUSD;
    if (!g_dailyLossHit && dailyTotal <= -dailyLossLimit)
    {
        CloseAllPositions();
        g_dailyLossHit = true;
        Print("Daily loss limit đạt: ", DoubleToString(dailyTotal, 2), " / -",
              DoubleToString(DailyLossUSD, 2), " USD - Dừng hôm nay.");
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
    int now   = dt.hour * 60 + dt.min;
    int start = StartHour * 60 + StartMinute;
    int end   = EndHour   * 60 + EndMinute;
    return now >= start && now < end;
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
    g_buyCount   = 0;
    g_sellCount  = 0;

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
            g_buyCount++;
        }
        else
        {
            g_sellProfit += profit;
            g_sellLots   += vol;
            g_sellCount++;
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
    if (!IsInTradingHours() && g_totalPos == 0) return;
    if (g_totalPos >= MaxPositions) return;
    if (g_adxValue < ADXMinStrength) return;

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
    if (g_dailyTargetHit || g_dailyLossHit)
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
    return lowerWick > body * 2 && lowerWick * 2 > upperWick;
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
