//+------------------------------------------------------------------+
//|                                             DCA_Basket_EA.mq5    |
//|              DCA Basket 2 chiều - XAUUSD                         |
//+------------------------------------------------------------------+
#property copyright "LongHD"
#property version   "1.13"
#property strict

#include <Trade\Trade.mqh>
#include <Trade\OrderInfo.mqh>
#include <Trade\PositionInfo.mqh>

input group "========== 01. SEMI-AUTO MANUAL MODE =========="
input bool   InpSemiAutoEnabled  = false;     // Bật chế độ bán tự động (lệnh tay)
input int    InpSemiAutoTP       = 200;       // TP mỗi lệnh tính từ giá vào (pip), 0 = không đặt
input int    InpSemiAutoSL       = 500;       // SL mỗi lệnh tính từ giá vào (pip), 0 = không đặt
input double InpSemiAutoTotalTP  = 0.0;       // Tổng lãi tự đóng hết lệnh ($), 0 = tắt
input double InpSemiAutoTotalSL  = 0.0;       // Tổng lỗ tự đóng hết lệnh ($), 0 = tắt

input group "========== 02. MARKET FILTER - ATR/ADX =========="
input bool             InpFilterEnabled  = true;      // Bật bộ lọc ATR/ADX trước khi vào lệnh
input ENUM_TIMEFRAMES  InpFilterTF       = PERIOD_M5; // Khung thời gian ATR/ADX
input int    InpATRPeriod      = 5;     // Chu kỳ ATR
input double InpATRMax         = 6.0;   // Chỉ vào lệnh khi ATR < giá trị này
input int    InpADXPeriod      = 5;     // Chu kỳ ADX
input double InpADXMax         = 20.0;  // Chỉ vào lệnh khi ADX < giá trị này

input group "========== 03. CORE =========="
input bool    InpManualTrigger    = false;  // Chờ lệnh tay để kích hoạt, đóng khi lệnh tay đóng hết
input double  InpInitialLot       = 0.01;   // Lot ban đầu
input int     InpInitialDistance  = 100;    // Khoảng cách pending ban đầu
input int     InpTrailingDistance = 300;    // Khoảng cách trailing pending ngược chiều
input double  InpTotalProfitTP    = 5.0;    // Tổng lợi nhuận tiền để đóng tất cả lệnh
input int     InpMaxSpread        = 0;      // Spread tối đa, 0 = không lọc
input int     InpSlippage         = 30;     // Slippage/deviation

input group "========== 04. DCA ÂM - HỆ SỐ CỘNG =========="
input int     InpDCAMinDistance   = 100;  // Khoảng cách DCA âm tối thiểu so với lệnh gần nhất
input double  InpDCALotAdd        = 0.01; // Lot cộng thêm từ lệnh cùng chiều gần nhất
input int     InpMaxOrdersPerSide = 20;   // Số lệnh tối đa mỗi phía Buy/Sell
input int     InpMinRestDCA       = 1;    // Nghỉ tối thiểu giữa 2 lệnh DCA cùng phía (giây)

input group "========== 04B. DCA MULTIPLIER KHI ÂM TÀI KHOẢN =========="
input bool    InpDCAMultEnabled    = true; // Bật hệ số nhân khi âm % tài khoản
input double  InpDCAMultTriggerPct = 5.0;  // Âm bao nhiêu % balance thì chuyển sang hệ số nhân
input double  InpDCAMultFactor     = 1.5;  // Hệ số nhân lot DCA khi kích hoạt
input double  InpDCAMaxLot         = 0.0;  // Giới hạn lot DCA tối đa

input group "========== 05. OPTIONAL PROTECTION =========="
input double  InpPendingTP       = 0;  // TP riêng cho pending, 0 = tắt
input int     InpMinModifyPoints = 1;  // Buộc tối thiểu để modify pending (điểm)

input group "========== 06. FAST BASKET CLOSE =========="
input int     InpFastCloseLoops  = 8;    // Số vòng quét đóng nhanh khi đạt tổng tiền
input bool    InpAsyncClose      = true; // true = gửi lệnh đóng async nhanh hơn
input int     InpTimerMS         = 250;  // Timer tiếp tục quét khi không có tick, 100-1000ms
input int     InpWaitNewCycleSec = 5;    // Đợi x giây trước khi tạo chu kỳ pending mới

input group "========== 07. RED NEWS PAUSE - MT5 CALENDAR =========="
input bool    InpNewsPauseEnabled = true;             // Tạm dừng EA khi sắp có tin tức
input int     InpNewsPauseBefore  = 30;               // Dừng trước tin x phút
input int     InpNewsResumeAfter  = 30;               // Mở lại sau tin x phút
input bool    InpNewsFilterA      = true;             // Lọc tin độ phiên Á
input string  InpNewsCurrA        = "JPY,CNY,AUD,NZD"; // Tiền tệ phiên Á
input bool    InpNewsFilterAu     = true;             // Lọc tin độ phiên Âu
input string  InpNewsCurrAu       = "EUR,GBP,CHF";   // Tiền tệ phiên Âu
input bool    InpNewsFilterMy     = true;             // Lọc tin độ phiên Mỹ
input string  InpNewsCurrMy       = "USD,CAD";        // Tiền tệ phiên Mỹ
input int     InpNewsScanHours    = 24;               // Khoảng quét lịch tin sắp tới (giờ)
input int     InpNewsUpdateSec    = 60;               // Tần suất cập nhật lịch tin (giây)
input bool    InpDeletePendingNews= true;             // Xóa pending khi đang trong vùng tin

input group "========== 08. CAPITAL GUARD - LÃI/LỖ THEO VỐN KHỞI CHẠY =========="
input bool    InpCapGuardProfit   = true;  // Dừng EA khi lãi x% vốn khởi chạy
input double  InpCapGuardProfitPct= 5.0;   // Lãi x% thì đóng sạch và tạm dừng
input bool    InpCapGuardLoss     = true;  // Dừng EA khi âm trạng thái x% vốn
input double  InpCapGuardLossPct  = 10.0;  // Âm x% vốn thì đóng sạch và tạm dừng
input int     InpCapGuardRestMin  = 60;    // Nghỉ bao nhiêu phút rồi tự vào lại (0 = không tự vào)
input bool    InpManageAllMagic0  = true;  // Quản lý cả lệnh tay magic 0 trên cùng symbol

input group "========== 09. WEEKEND CLOSE =========="
input bool    InpWeekendClose     = true; // Đóng tất cả lệnh cuối tuần (thứ 6)
input int     InpWeekendCloseHour = 21;   // Giờ đóng thứ 6 (server time)
input int     InpWeekendCloseMin  = 0;    // Phút đóng thứ 6

input group "========== 10. PANEL HIỂN THỊ =========="
input int     InpPanelX     = 5;    // Vị trí X panel (pixel từ trái)
input int     InpPanelY     = 48;   // Vị trí Y panel (pixel từ trên)
input int     InpPanelW     = 350;  // Chiều rộng panel
input int     InpFontSize   = 9;    // Cỡ chữ
input int     InpLineSpacing= 16;   // Khoảng cách dòng (pixel)
input int     InpPanel1H    = 340;  // Chiều cao panel 1 (0 = tự tính)
input int     InpPanel2H    = 150;  // Chiều cao panel 2 (0 = tự tính)
input int     InpPanelGap   = 5;    // Khoảng cách giữa 2 panel (pixel)

CTrade        g_trade;
CPositionInfo g_pos;
COrderInfo    g_ord;

#define MAGIC_NUMBER 26051501

double   g_startCapital        = 0;
bool     g_capGuardActive      = false;
bool     g_guardSeenOrder      = false;
datetime g_capGuardResumeAt    = 0;
bool     g_weekendClosed       = false;
double   g_dayStartEquity      = 0;
int      g_lastDay             = -1;
datetime g_lastDCATimeBuy      = 0;
datetime g_lastDCATimeSell     = 0;
datetime g_nextCycleAllowAt    = 0;
double   g_lastDCATriggerBuy   = 0;
double   g_lastDCATriggerSell  = 0;
double   g_lastDCALotBuy       = 0;
double   g_lastDCALotSell      = 0;
datetime g_lastReplaceSellTime = 0;
datetime g_lastReplaceBuyTime  = 0;
double   g_pip                 = 0;
bool     g_manualTriggerActive = false;
bool     g_darkMode            = true;

int      g_atrHandle = INVALID_HANDLE;
int      g_adxHandle = INVALID_HANDLE;
double   g_lastATR   = 0;
double   g_lastADX   = 0;

const string GUI        = "DCA_";
bool         g_semiAutoMode = false;

double   g_lotStep = 0.01;
double   g_lotMin  = 0.01;
double   g_lotMax  = 100.0;

struct NewsItem { datetime time; string currency; };
NewsItem  g_newsCache[];
datetime  g_lastNewsUpdate = 0;

int OnInit()
{
    g_trade.SetExpertMagicNumber(MAGIC_NUMBER);
    g_trade.SetDeviationInPoints(InpSlippage);
    g_trade.SetAsyncMode(InpAsyncClose);
    g_trade.SetTypeFilling(ORDER_FILLING_RETURN);

    int digits = (int)SymbolInfoInteger(_Symbol, SYMBOL_DIGITS);
    g_pip = _Point * ((digits == 3 || digits == 5) ? 10.0 : 1.0);

    g_startCapital = AccountInfoDouble(ACCOUNT_BALANCE);
    g_semiAutoMode = InpSemiAutoEnabled;

    g_lotStep = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
    g_lotMin  = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
    g_lotMax  = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);

    EventSetMillisecondTimer(InpTimerMS);

    if(InpFilterEnabled)
    {
        g_atrHandle = iATR(_Symbol, InpFilterTF, InpATRPeriod);
        g_adxHandle = iADX(_Symbol, InpFilterTF, InpADXPeriod);
        if(g_atrHandle == INVALID_HANDLE || g_adxHandle == INVALID_HANDLE)
        {
            Print("Market filter: failed to create indicator handles");
            return INIT_FAILED;
        }
    }

    return INIT_SUCCEEDED;
}

void OnDeinit(const int reason)
{
    EventKillTimer();
    if(g_atrHandle != INVALID_HANDLE) IndicatorRelease(g_atrHandle);
    if(g_adxHandle != INVALID_HANDLE) IndicatorRelease(g_adxHandle);
    ObjectsDeleteAll(0, GUI);
}

void OnTick()  { RunEA(); UpdateGUI(); }
void OnTimer() { RunEA(); UpdateGUI(); }

void OnChartEvent(const int id, const long& lp, const double& dp, const string& sp)
{
    if(id != CHARTEVENT_OBJECT_CLICK) return;
    if(sp == GUI + "BtnToggleMode") g_semiAutoMode = !g_semiAutoMode;
    if(sp == GUI + "BtnCloseAll")  CloseAllOrders();
    if(sp == GUI + "BtnTheme")     g_darkMode     = !g_darkMode;
    ObjectSetInteger(0, sp, OBJPROP_STATE, false);
    UpdateGUI();
}

void RunEA()
{
    if(CheckCapitalGuard()) return;
    if(CheckWeekendClose()) return;

    if(InpManualTrigger)
    {
        int magic0Pos = CountMagic0Positions();
        if(g_manualTriggerActive && magic0Pos == 0)
        {
            CloseAllOrders();
            g_manualTriggerActive = false;
            return;
        }
        if(magic0Pos > 0) g_manualTriggerActive = true;
    }

    int buyPos   = CountPositions(POSITION_TYPE_BUY);
    int sellPos  = CountPositions(POSITION_TYPE_SELL);
    int buyPend  = CountPending(ORDER_TYPE_BUY_STOP);
    int sellPend = CountPending(ORDER_TYPE_SELL_STOP);
    int totalPos = buyPos + sellPos;

    if(totalPos > 0)
    {
        double totalProfit = GetTotalProfit();

        if(!g_semiAutoMode && totalProfit >= InpTotalProfitTP)
        {
            CloseAllOrders();
            g_nextCycleAllowAt = TimeCurrent() + InpWaitNewCycleSec;
            return;
        }

        if(g_semiAutoMode)
        {
            if(InpSemiAutoTotalTP > 0 && totalProfit >= InpSemiAutoTotalTP)
                { CloseAllOrders(); return; }
            if(InpSemiAutoTotalSL > 0 && totalProfit <= -InpSemiAutoTotalSL)
                { CloseAllOrders(); return; }
        }
    }

    if(TimeCurrent() < g_nextCycleAllowAt) return;
    if(!CheckSpread()) return;

    if(InpNewsPauseEnabled)
    {
        UpdateNewsCache();
        if(IsInNewsZone())
        {
            if(InpDeletePendingNews) DeleteAllPending();
            return;
        }
    }

    bool marketOk = CheckMarketFilter();

    if(totalPos == 0 && buyPend == 0 && sellPend == 0)
    {
        if(!g_semiAutoMode && !InpManualTrigger && marketOk) PlaceInitialPending();
        return;
    }

    if(totalPos > 0)
    {
        if(g_semiAutoMode)
        {
            ManageSemiAutoPositions();
            return;
        }

        if(buyPos > 0 && buyPos < InpMaxOrdersPerSide)
            ManageDCA(POSITION_TYPE_BUY);

        if(sellPos > 0 && sellPos < InpMaxOrdersPerSide)
            ManageDCA(POSITION_TYPE_SELL);

        ManageAutoBasketTP();
        TrailOppositePending(buyPos, sellPos, buyPend, sellPend);
    }
}

bool CheckMarketFilter()
{
    if(!InpFilterEnabled) return true;
    if(g_atrHandle == INVALID_HANDLE || g_adxHandle == INVALID_HANDLE) return true;

    double atrBuf[], adxBuf[];
    ArraySetAsSeries(atrBuf, true);
    ArraySetAsSeries(adxBuf, true);
    if(CopyBuffer(g_atrHandle, 0, 0, 1, atrBuf) < 1) return true;
    if(CopyBuffer(g_adxHandle, 0, 0, 1, adxBuf) < 1) return true;

    g_lastATR = atrBuf[0];
    g_lastADX = adxBuf[0];

    if(g_lastATR >= InpATRMax || g_lastADX >= InpADXMax)
        return false;
    return true;
}

void ManageSemiAutoPositions()
{
    double tickSize  = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
    double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);

    double tpBuyLevel = 0, tpSellLevel = 0;
    double slBuyLevel = 0, slSellLevel = 0;

    if(tickValue > 0 && tickSize > 0)
    {
        double buyWtd = 0, buyLot = 0, sellWtd = 0, sellLot = 0;
        for(int i = PositionsTotal() - 1; i >= 0; i--)
        {
            if(!g_pos.SelectByIndex(i)) continue;
            if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
            double vol = g_pos.Volume(), op = g_pos.PriceOpen();
            if(g_pos.PositionType() == POSITION_TYPE_BUY)
                { buyWtd += op * vol; buyLot += vol; }
            else
                { sellWtd += op * vol; sellLot += vol; }
        }

        // Mixed BUY+SELL: broker TP 2 chiều ngược hướng giá, không thể cùng hit.
        // Software monitoring trong RunEA xử lý tổng hợp.
        bool hasMixed = (buyLot > 0 && sellLot > 0);

        if(!hasMixed && buyLot > 0)
        {
            double distTP  = (InpSemiAutoTotalTP > 0) ? InpSemiAutoTotalTP * tickSize / (buyLot * tickValue) : 0;
            double distSL  = (InpSemiAutoTotalSL > 0) ? InpSemiAutoTotalSL * tickSize / (buyLot * tickValue) : 0;
            double avgOpen = buyWtd / buyLot;
            if(distTP > 0) tpBuyLevel = NormalizePrice(avgOpen + distTP);
            if(distSL > 0) slBuyLevel = NormalizePrice(avgOpen - distSL);
        }
        if(!hasMixed && sellLot > 0)
        {
            double distTP  = (InpSemiAutoTotalTP > 0) ? InpSemiAutoTotalTP * tickSize / (sellLot * tickValue) : 0;
            double distSL  = (InpSemiAutoTotalSL > 0) ? InpSemiAutoTotalSL * tickSize / (sellLot * tickValue) : 0;
            double avgOpen = sellWtd / sellLot;
            if(distTP > 0) tpSellLevel = NormalizePrice(avgOpen - distTP);
            if(distSL > 0) slSellLevel = NormalizePrice(avgOpen + distSL);
        }
    }

    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;

        bool   isBuy = (g_pos.PositionType() == POSITION_TYPE_BUY);
        double open  = g_pos.PriceOpen();

        double tpInd = (InpSemiAutoTP > 0)
                       ? NormalizePrice(isBuy ? open + InpSemiAutoTP * g_pip
                                              : open - InpSemiAutoTP * g_pip)
                       : 0;
        double tpBsk = isBuy ? tpBuyLevel : tpSellLevel;
        double tp;
        if(tpInd > 0 && tpBsk > 0)
            tp = isBuy ? MathMin(tpInd, tpBsk)  // BUY: thấp hơn = gần hơn
                       : MathMax(tpInd, tpBsk); // SELL: cao hơn = gần hơn
        else if(tpBsk > 0) tp = tpBsk;
        else                tp = tpInd;

        double slInd = (InpSemiAutoSL > 0)
                       ? NormalizePrice(isBuy ? open - InpSemiAutoSL * g_pip
                                              : open + InpSemiAutoSL * g_pip)
                       : 0;
        double slBsk = isBuy ? slBuyLevel : slSellLevel;
        double sl;
        if(slInd > 0 && slBsk > 0)
            sl = isBuy ? MathMax(slInd, slBsk)  // BUY: cao hơn = gần hơn
                       : MathMin(slInd, slBsk); // SELL: thấp hơn = gần hơn
        else if(slBsk > 0) sl = slBsk;
        else                sl = slInd;

        // Nếu không tính được giá trị mới thì giữ nguyên giá trị cũ, tránh xóa TP/SL đã đặt
        double finalTP = (tp > 0) ? tp : g_pos.TakeProfit();
        double finalSL = (sl > 0) ? sl : g_pos.StopLoss();

        if(MathAbs(g_pos.TakeProfit() - finalTP) > _Point * 0.5 ||
           MathAbs(g_pos.StopLoss()   - finalSL) > _Point * 0.5)
            g_trade.PositionModify(g_pos.Ticket(), finalSL, finalTP);
    }
}

void PlaceInitialPending()
{
    double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);

    double buyPrice  = NormalizePrice(ask + InpInitialDistance * g_pip);
    double sellPrice = NormalizePrice(bid - InpInitialDistance * g_pip);

    double tpBuy = 0, tpSell = 0;
    if(InpPendingTP > 0)
    {
        tpBuy  = NormalizePrice(buyPrice  + InpPendingTP * g_pip);
        tpSell = NormalizePrice(sellPrice - InpPendingTP * g_pip);
    }

    bool ok1 = g_trade.BuyStop(InpInitialLot, buyPrice, _Symbol, 0, tpBuy,
                                ORDER_TIME_GTC, 0, "INIT_BUY_STOP");
    bool ok2 = g_trade.SellStop(InpInitialLot, sellPrice, _Symbol, 0, tpSell,
                                 ORDER_TIME_GTC, 0, "INIT_SELL_STOP");

    if(ok1 || ok2)
        g_nextCycleAllowAt = TimeCurrent() + 2;
}

void ManageDCA(ENUM_POSITION_TYPE dir)
{
    double bid     = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double ask     = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double dcaDist = InpDCAMinDistance * g_pip;

    // BUY thực thi tại ASK, SELL tại BID — đo khoảng cách theo giá thực thi để bước đều
    double refPrice = (dir == POSITION_TYPE_BUY) ? ask : bid;

    datetime lastDCA = (dir == POSITION_TYPE_BUY) ? g_lastDCATimeBuy : g_lastDCATimeSell;
    if(TimeCurrent() - lastDCA < InpMinRestDCA) return;

    double lastTrigger = (dir == POSITION_TYPE_BUY) ? g_lastDCATriggerBuy : g_lastDCATriggerSell;
    if(lastTrigger > 0)
    {
        if(dir == POSITION_TYPE_BUY  && refPrice > lastTrigger - dcaDist) return;
        if(dir == POSITION_TYPE_SELL && refPrice < lastTrigger + dcaDist) return;
    }
    else
    {
        double extremePrice = GetExtremePositionPrice(dir);
        if(extremePrice <= 0) return;
        if(dir == POSITION_TYPE_BUY  && refPrice > extremePrice - dcaDist) return;
        if(dir == POSITION_TYPE_SELL && refPrice < extremePrice + dcaDist) return;
    }

    double lastLot = GetExtremeLot(dir);
    double mult    = GetDCAMultiplier();
    double newLot  = NormalizeLot(lastLot + InpDCALotAdd * mult);
    if(InpDCAMaxLot > 0 && newLot > InpDCAMaxLot)
        newLot = NormalizeLot(InpDCAMaxLot);

    string comment = (dir == POSITION_TYPE_BUY) ? "DCA_BUY" : "DCA_SELL";
    bool ok = (dir == POSITION_TYPE_BUY) ? g_trade.Buy(newLot, _Symbol, 0, 0, 0, comment)
                                         : g_trade.Sell(newLot, _Symbol, 0, 0, 0, comment);
    if(ok)
    {
        if(dir == POSITION_TYPE_BUY)
        {
            g_lastDCATriggerBuy = ask;
            g_lastDCATimeBuy    = TimeCurrent();
            g_lastDCALotBuy     = newLot;
        }
        else
        {
            g_lastDCATriggerSell = bid;
            g_lastDCATimeSell    = TimeCurrent();
            g_lastDCALotSell     = newLot;
        }
    }
}

void ManageAutoBasketTP()
{
    double tickSize  = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
    double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
    if(tickValue <= 0 || tickSize <= 0) return;

    double buyWtd = 0, buyLot = 0, sellWtd = 0, sellLot = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
        double vol = g_pos.Volume(), op = g_pos.PriceOpen();
        if(g_pos.PositionType() == POSITION_TYPE_BUY)
            { buyWtd += op * vol; buyLot += vol; }
        else
            { sellWtd += op * vol; sellLot += vol; }
    }

    double tpBuyLevel = 0, tpSellLevel = 0;
    if(buyLot > 0)
    {
        double dist = InpTotalProfitTP * tickSize / (buyLot * tickValue);
        tpBuyLevel  = NormalizePrice(buyWtd / buyLot + dist);
    }
    if(sellLot > 0)
    {
        double dist = InpTotalProfitTP * tickSize / (sellLot * tickValue);
        tpSellLevel = NormalizePrice(sellWtd / sellLot - dist);
    }

    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;

        bool   isBuy = (g_pos.PositionType() == POSITION_TYPE_BUY);
        double tp    = isBuy ? tpBuyLevel : tpSellLevel;
        double sl    = g_pos.StopLoss();

        if(tp > 0 && MathAbs(g_pos.TakeProfit() - tp) > _Point * 0.5)
            g_trade.PositionModify(g_pos.Ticket(), sl, tp);
    }
}

void TrailOppositePending(int buyPos, int sellPos, int buyPend, int sellPend)
{
    double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);

    double tickSize  = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
    double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
    double tpDist    = (tickValue > 0 && InpInitialLot > 0)
                       ? NormalizePrice(InpTotalProfitTP * tickSize / (tickValue * InpInitialLot))
                       : 0;

    if(buyPos > 0 && sellPos == 0)
    {
        double newStop = NormalizePrice(bid - InpTrailingDistance * g_pip);
        if(sellPend == 0)
        {
            if(TimeCurrent() - g_lastReplaceSellTime >= 2)
            {
                double tpSell = (tpDist > 0) ? NormalizePrice(newStop - tpDist) : 0;
                g_trade.SellStop(InpInitialLot, newStop, _Symbol, 0, tpSell,
                                 ORDER_TIME_GTC, 0, "TRAIL_SELL_STOP");
                g_lastReplaceSellTime = TimeCurrent();
            }
        }
        else
        {
            double currentStop = GetPendingPrice(ORDER_TYPE_SELL_STOP);
            if(newStop > currentStop + InpMinModifyPoints * g_pip)
                ModifyPendingStop(ORDER_TYPE_SELL_STOP, newStop);
        }
    }

    if(sellPos > 0 && buyPos == 0)
    {
        double newStop = NormalizePrice(ask + InpTrailingDistance * g_pip);
        if(buyPend == 0)
        {
            if(TimeCurrent() - g_lastReplaceBuyTime >= 2)
            {
                double tpBuy = (tpDist > 0) ? NormalizePrice(newStop + tpDist) : 0;
                g_trade.BuyStop(InpInitialLot, newStop, _Symbol, 0, tpBuy,
                                ORDER_TIME_GTC, 0, "TRAIL_BUY_STOP");
                g_lastReplaceBuyTime = TimeCurrent();
            }
        }
        else
        {
            double currentStop = GetPendingPrice(ORDER_TYPE_BUY_STOP);
            if(newStop < currentStop - InpMinModifyPoints * g_pip)
                ModifyPendingStop(ORDER_TYPE_BUY_STOP, newStop);
        }
    }
}

void ModifyPendingStop(ENUM_ORDER_TYPE type, double newPrice)
{
    for(int i = OrdersTotal() - 1; i >= 0; i--)
    {
        if(!g_ord.SelectByIndex(i)) continue;
        if(g_ord.Magic() != MAGIC_NUMBER || g_ord.Symbol() != _Symbol) continue;
        if(g_ord.OrderType() != type) continue;
        g_trade.OrderModify(g_ord.Ticket(), newPrice, 0, g_ord.TakeProfit(), ORDER_TIME_GTC, 0);
        return;
    }
}

void CloseAllOrders()
{
    for(int attempt = 0; attempt < InpFastCloseLoops; attempt++)
    {
        bool anyOpen = false;
        for(int i = PositionsTotal() - 1; i >= 0; i--)
        {
            if(!g_pos.SelectByIndex(i)) continue;
            if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
            anyOpen = true;
            g_trade.PositionClose(g_pos.Ticket());
        }
        if(!anyOpen) break;
        Sleep(50);
    }
    DeleteAllPending();

    g_lastDCATriggerBuy   = 0;
    g_lastDCATriggerSell  = 0;
    g_lastDCATimeBuy      = 0;
    g_lastDCATimeSell     = 0;
    g_lastDCALotBuy       = 0;
    g_lastDCALotSell      = 0;
    g_lastReplaceSellTime = 0;
    g_lastReplaceBuyTime  = 0;
}

void DeleteAllPending()
{
    for(int i = OrdersTotal() - 1; i >= 0; i--)
    {
        if(!g_ord.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_ord.Magic(), g_ord.Symbol())) continue;
        g_trade.OrderDelete(g_ord.Ticket());
    }
}

void UpdateNewsCache()
{
    if(TimeCurrent() - g_lastNewsUpdate < InpNewsUpdateSec) return;
    g_lastNewsUpdate = TimeCurrent();

    ArrayResize(g_newsCache, 0);

    datetime from = TimeCurrent() - (datetime)(InpNewsPauseBefore * 60);
    datetime to   = TimeCurrent() + (datetime)(InpNewsScanHours   * 3600);

    MqlCalendarValue values[];
    int total = CalendarValueHistory(values, from, to, NULL, NULL);
    if(total <= 0) return;

    string currencies[];
    BuildCurrencyList(currencies);

    for(int i = 0; i < total; i++)
    {
        MqlCalendarEvent event;
        if(!CalendarEventById(values[i].event_id, event)) continue;
        if(event.importance != CALENDAR_IMPORTANCE_HIGH) continue;

        MqlCalendarCountry country;
        if(!CalendarCountryById(event.country_id, country)) continue;
        if(!IsCurrencyInList(country.currency, currencies)) continue;

        int idx = ArraySize(g_newsCache);
        ArrayResize(g_newsCache, idx + 1);
        g_newsCache[idx].time     = values[i].time;
        g_newsCache[idx].currency = country.currency;
    }
}

bool IsInNewsZone()
{
    datetime now         = TimeCurrent();
    datetime pauseBefore = (datetime)(InpNewsPauseBefore * 60);
    datetime resumeAfter = (datetime)(InpNewsResumeAfter * 60);

    for(int i = 0; i < ArraySize(g_newsCache); i++)
    {
        datetime newsTime = g_newsCache[i].time;
        if(now >= newsTime - pauseBefore && now <= newsTime + resumeAfter)
            return true;
    }
    return false;
}

void BuildCurrencyList(string &list[])
{
    ArrayResize(list, 0);
    if(InpNewsFilterA)  AddCurrencies(list, InpNewsCurrA);
    if(InpNewsFilterAu) AddCurrencies(list, InpNewsCurrAu);
    if(InpNewsFilterMy) AddCurrencies(list, InpNewsCurrMy);
}

void AddCurrencies(string &list[], string csv)
{
    string parts[];
    int n = StringSplit(csv, StringGetCharacter(",", 0), parts);
    for(int i = 0; i < n; i++)
    {
        StringTrimRight(parts[i]);
        StringTrimLeft(parts[i]);
        if(parts[i] == "") continue;
        int idx = ArraySize(list);
        ArrayResize(list, idx + 1);
        list[idx] = parts[i];
    }
}

bool IsCurrencyInList(string currency, const string &list[])
{
    for(int i = 0; i < ArraySize(list); i++)
        if(list[i] == currency) return true;
    return false;
}

bool CheckWeekendClose()
{
    if(!InpWeekendClose) return false;

    MqlDateTime dt;
    TimeToStruct(TimeTradeServer(), dt);

    bool isWeekend = (dt.day_of_week == 5 &&
                      (dt.hour > InpWeekendCloseHour ||
                       (dt.hour == InpWeekendCloseHour && dt.min >= InpWeekendCloseMin)))
                  || dt.day_of_week == 6
                  || dt.day_of_week == 0;

    if(!isWeekend) { g_weekendClosed = false; return false; }
    if(!g_weekendClosed) { CloseAllOrders(); g_weekendClosed = true; }
    return true;
}

bool CheckCapitalGuard()
{
    if(!InpCapGuardProfit && !InpCapGuardLoss) return false;

    if(g_capGuardActive)
    {
        if(InpCapGuardRestMin > 0 && g_capGuardResumeAt > 0 &&
           TimeCurrent() >= g_capGuardResumeAt)
        {
            g_capGuardActive   = false;
            g_guardSeenOrder   = false;
            g_capGuardResumeAt = 0;
            g_startCapital     = AccountInfoDouble(ACCOUNT_BALANCE);
            return false;
        }
        return g_capGuardActive;
    }

    double balance   = AccountInfoDouble(ACCOUNT_BALANCE);
    double equity    = AccountInfoDouble(ACCOUNT_EQUITY);
    if(g_startCapital <= 0) g_startCapital = balance;
    if(g_startCapital <= 0) return false;

    double changePct = (equity - g_startCapital) / g_startCapital * 100.0;

    if(InpCapGuardProfit && changePct >= InpCapGuardProfitPct)
    {
        CloseAllOrders();
        g_capGuardActive   = true;
        g_guardSeenOrder   = false;
        g_capGuardResumeAt = (InpCapGuardRestMin > 0)
                             ? TimeCurrent() + InpCapGuardRestMin * 60 : 0;
        return true;
    }
    if(InpCapGuardLoss && changePct <= -InpCapGuardLossPct)
    {
        CloseAllOrders();
        g_capGuardActive   = true;
        g_guardSeenOrder   = false;
        g_capGuardResumeAt = (InpCapGuardRestMin > 0)
                             ? TimeCurrent() + InpCapGuardRestMin * 60 : 0;
        return true;
    }
    return false;
}

double GetTotalProfit()
{
    double total = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
        total += g_pos.Profit() + g_pos.Swap() + g_pos.Commission();
    }
    return total;
}

int CountPositions(ENUM_POSITION_TYPE type)
{
    int count = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
        if(g_pos.PositionType() == type) count++;
    }
    return count;
}

int CountPending(ENUM_ORDER_TYPE type)
{
    int count = 0;
    for(int i = OrdersTotal() - 1; i >= 0; i--)
    {
        if(!g_ord.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_ord.Magic(), g_ord.Symbol())) continue;
        if(g_ord.OrderType() == type) count++;
    }
    return count;
}

double GetExtremePositionPrice(ENUM_POSITION_TYPE dir)
{
    double extreme = -1;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
        if(g_pos.PositionType() != dir) continue;
        double price = g_pos.PriceOpen();
        if(extreme < 0) { extreme = price; continue; }
        if(dir == POSITION_TYPE_BUY  && price < extreme) extreme = price;
        if(dir == POSITION_TYPE_SELL && price > extreme) extreme = price;
    }
    return extreme;
}

double GetExtremeLot(ENUM_POSITION_TYPE dir)
{
    double ep = GetExtremePositionPrice(dir);

    // Async: lệnh DCA chưa visible trong danh sách, dùng lot đã lưu để chuỗi lot đúng thứ tự
    if(dir == POSITION_TYPE_BUY && g_lastDCALotBuy > 0 && g_lastDCATriggerBuy > 0)
    {
        if(ep < 0 || g_lastDCATriggerBuy < ep)
            return g_lastDCALotBuy;
    }
    if(dir == POSITION_TYPE_SELL && g_lastDCALotSell > 0 && g_lastDCATriggerSell > 0)
    {
        if(ep < 0 || g_lastDCATriggerSell > ep)
            return g_lastDCALotSell;
    }

    if(ep < 0) return InpInitialLot;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
        if(g_pos.PositionType() != dir) continue;
        if(MathAbs(g_pos.PriceOpen() - ep) < _Point * 0.5)
            return g_pos.Volume();
    }
    return InpInitialLot;
}

double GetPendingPrice(ENUM_ORDER_TYPE type)
{
    for(int i = OrdersTotal() - 1; i >= 0; i--)
    {
        if(!g_ord.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_ord.Magic(), g_ord.Symbol())) continue;
        if(g_ord.OrderType() == type) return g_ord.PriceOpen();
    }
    return 0;
}

double GetDCAMultiplier()
{
    if(!InpDCAMultEnabled) return 1.0;
    double bal = AccountInfoDouble(ACCOUNT_BALANCE);
    double eq  = AccountInfoDouble(ACCOUNT_EQUITY);
    if(bal <= 0) return 1.0;
    double lossPct = (bal - eq) / bal * 100.0;
    if(lossPct >= InpDCAMultTriggerPct) return InpDCAMultFactor;
    return 1.0;
}

bool CheckSpread()
{
    if(InpMaxSpread <= 0) return true;
    return (long)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD) <= InpMaxSpread;
}

bool IsOurOrder(long magic, string symbol)
{
    if(symbol != _Symbol) return false;
    if(magic == MAGIC_NUMBER) return true;
    if(magic == 0 && (InpManageAllMagic0 || g_semiAutoMode || InpManualTrigger)) return true;
    return false;
}

int CountMagic0Positions()
{
    int count = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(g_pos.Symbol() != _Symbol) continue;
        if(g_pos.Magic() == 0) count++;
    }
    return count;
}

void Lbl(string name, string text, int x, int y, color clr = clrSilver, int sz = 9)
{
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0)
    {
        ObjectCreate(0, obj, OBJ_LABEL, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,     CORNER_LEFT_UPPER);
        ObjectSetInteger(0, obj, OBJPROP_BACK,       false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE, false);
    }
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x);
    ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
    ObjectSetString(0,  obj, OBJPROP_TEXT,      text);
    ObjectSetString(0,  obj, OBJPROP_FONT,      "Courier New");
    ObjectSetInteger(0, obj, OBJPROP_COLOR,     clr);
    ObjectSetInteger(0, obj, OBJPROP_FONTSIZE,  sz);
}

void CreateBtn(string name, string text, int x, int y, int w, int h, color bg, color border = clrSilver)
{
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0)
    {
        ObjectCreate(0, obj, OBJ_BUTTON, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,     CORNER_LEFT_UPPER);
        ObjectSetString(0,  obj, OBJPROP_FONT,       "Courier New");
        ObjectSetInteger(0, obj, OBJPROP_FONTSIZE,   9);
        ObjectSetInteger(0, obj, OBJPROP_BACK,       false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE, false);
    }
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE,  x);
    ObjectSetInteger(0, obj, OBJPROP_YDISTANCE,  y);
    ObjectSetInteger(0, obj, OBJPROP_XSIZE,      w);
    ObjectSetInteger(0, obj, OBJPROP_YSIZE,      h);
    ObjectSetString(0,  obj, OBJPROP_TEXT,         text);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,        clrWhite);
    ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,      bg);
    ObjectSetInteger(0, obj, OBJPROP_BORDER_COLOR, border);
    ObjectSetInteger(0, obj, OBJPROP_STATE,        false);
}

void CreateBG(string name, int x, int y, int w, int h, color bg, color border)
{
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0)
    {
        ObjectCreate(0, obj, OBJ_RECTANGLE_LABEL, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
        ObjectSetInteger(0, obj, OBJPROP_BORDER_TYPE, BORDER_FLAT);
        ObjectSetInteger(0, obj, OBJPROP_WIDTH,       1);
        ObjectSetInteger(0, obj, OBJPROP_BACK,        false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE,  false);
    }
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x);
    ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
    ObjectSetInteger(0, obj, OBJPROP_XSIZE,     w);
    ObjectSetInteger(0, obj, OBJPROP_YSIZE,     h);
    ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,   bg);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,     border);
}

void UpdateGUI()
{
    double balance = AccountInfoDouble(ACCOUNT_BALANCE);
    double equity  = AccountInfoDouble(ACCOUNT_EQUITY);
    double spread  = (double)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD);

    double totalProfit = 0, buyProfit = 0, sellProfit = 0;
    int    nBuy = 0, nSell = 0;
    double lotBuy = 0, lotSell = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
    {
        if(!g_pos.SelectByIndex(i)) continue;
        if(!IsOurOrder(g_pos.Magic(), g_pos.Symbol())) continue;
        double p = g_pos.Profit() + g_pos.Swap() + g_pos.Commission();
        totalProfit += p;
        if(g_pos.PositionType() == POSITION_TYPE_BUY)
            { buyProfit  += p; nBuy++;  lotBuy  += g_pos.Volume(); }
        else
            { sellProfit += p; nSell++; lotSell += g_pos.Volume(); }
    }
    double pnlPct = (balance > 0) ? totalProfit / balance * 100.0 : 0;

    MqlDateTime dt;
    TimeToStruct(TimeLocal(), dt);
    string tStr = StringFormat("%04d/%02d/%02d  %02d:%02d:%02d",
                               dt.year, dt.mon, dt.day, dt.hour, dt.min, dt.sec);

    color thBg1, thBd1, thBg2, thBd2, thTitle, thSep, thText, thDim;
    color thGreen, thGreenDim, thRed, thP2T, thSA1, thSA2;
    color thBtnCloseBg, thBtnCloseBd, thBtnThemeBg, thBtnThemeBd;
    if(g_darkMode)
    {
        thBg1=C'14,17,26';     thBd1=C'50,65,120';
        thBg2=C'17,21,32';     thBd2=C'65,90,160';
        thTitle=C'80,160,255'; thSep=C'45,58,105';
        thText=clrSilver;      thDim=C'80,80,80';
        thGreen=clrLimeGreen;  thGreenDim=C'100,200,100';
        thRed=clrTomato;
        thP2T=C'90,140,230';
        thSA1=C'220,150,50';   thSA2=C'180,120,40';
        thBtnCloseBg=C'20,60,150'; thBtnCloseBd=C'80,130,230';
        thBtnThemeBg=C'50,55,75';  thBtnThemeBd=C'110,120,160';
    }
    else
    {
        thBg1=C'238,242,255';  thBd1=C'110,140,215';
        thBg2=C'228,235,255';  thBd2=C'130,165,225';
        thTitle=C'20,60,200';  thSep=C'175,195,235';
        thText=C'30,35,65';    thDim=C'135,140,160';
        thGreen=C'0,155,0';    thGreenDim=C'0,120,0';
        thRed=C'195,30,30';
        thP2T=C'20,70,200';
        thSA1=C'160,85,0';     thSA2=C'130,65,0';
        thBtnCloseBg=C'50,100,220'; thBtnCloseBd=C'90,140,255';
        thBtnThemeBg=C'190,200,230'; thBtnThemeBd=C'110,140,215';
    }

    string modeName = g_semiAutoMode ? "SEMI-AUTO" : "AUTO";
    color  modeClr  = g_semiAutoMode ? clrOrange : thGreen;

    string tfName = EnumToString(InpFilterTF);
    StringReplace(tfName, "PERIOD_", "");
    string filterStr, filterValStr; color filterClr, filterValClr;
    if(!InpFilterEnabled) {
        filterStr    = "OFF";
        filterValStr = "  (disabled)";
        filterClr    = thDim; filterValClr = thDim;
    } else if(g_lastATR >= InpATRMax || g_lastADX >= InpADXMax) {
        filterStr    = "BLOCK [" + tfName + "]";
        filterValStr = StringFormat("  ATR=%.2f/%.1f  ADX=%.1f/%.0f",
                                    g_lastATR, InpATRMax, g_lastADX, InpADXMax);
        filterClr    = thRed; filterValClr = thRed;
    } else if(g_lastATR > 0) {
        filterStr    = "OK [" + tfName + "]";
        filterValStr = StringFormat("  ATR=%.2f/%.1f  ADX=%.1f/%.0f",
                                    g_lastATR, InpATRMax, g_lastADX, InpADXMax);
        filterClr    = thGreen; filterValClr = thGreenDim;
    } else {
        filterStr    = "Wait [" + tfName + "]";
        filterValStr = "  ATR=?  ADX=?";
        filterClr    = thText; filterValClr = thDim;
    }

    string newsStr; color newsClr;
    if(!InpNewsPauseEnabled)
        { newsStr = "OFF";    newsClr = thDim; }
    else if(IsInNewsZone())
        { newsStr = "PAUSED"; newsClr = thRed; }
    else
        { newsStr = "Clear";  newsClr = thGreen; }

    string guardStr; color guardClr;
    if(!g_capGuardActive) {
        guardStr = "OK"; guardClr = thGreen;
    } else if(g_capGuardResumeAt > 0) {
        int secsLeft = (int)(g_capGuardResumeAt - TimeCurrent());
        guardStr = (secsLeft > 0)
                   ? StringFormat("WAIT %dm%ds", secsLeft/60, secsLeft%60)
                   : "RESUMING...";
        guardClr = clrOrange;
    } else {
        guardStr = "STOPPED"; guardClr = thRed;
    }

    string wkndStr; color wkndClr;
    if(!InpWeekendClose)
        { wkndStr = "OFF";    wkndClr = thDim; }
    else if(g_weekendClosed)
        { wkndStr = "CLOSED"; wkndClr = thRed; }
    else
        { wkndStr = "OK";     wkndClr = thGreen; }

    if(g_lastDay != dt.day_of_year) {
        g_dayStartEquity = equity;
        g_lastDay        = dt.day_of_year;
    }
    double dailyPnl    = equity - g_dayStartEquity;
    double dailyPnlPct = (g_dayStartEquity > 0) ? dailyPnl / g_dayStartEquity * 100.0 : 0;

    color cTotal = (totalProfit >= 0) ? thGreen : thRed;
    color cBuy   = (buyProfit   >= 0) ? thGreen : thRed;
    color cSell  = (sellProfit  >= 0) ? thGreen : thRed;
    color cDaily = (dailyPnl    >= 0) ? thGreen : thRed;

    int pX  = InpPanelX;
    int pY  = InpPanelY;
    int pW  = InpPanelW;
    int s   = InpLineSpacing;
    int sz  = InpFontSize;
    int cx  = pX + 7;
    int sep = s - 3;
    int ttl = s + 1;

    int bg1H = (InpPanel1H > 0) ? InpPanel1H : 7 + ttl + sep + 7*s + sep + 4*s + sep + 2*s + s;
    int bg2Y = pY + bg1H + InpPanelGap;
    int bg2H = (InpPanel2H > 0) ? InpPanel2H : (g_semiAutoMode ? (s*5 + 20) : (s*3 + 10));

    CreateBG("BG1", pX, pY, pW, bg1H, thBg1, thBd1);

    int x = cx, y = pY + 7;
    Lbl("T",   "  Mr Kindly EA",               x, y, thTitle,     sz+1); y += ttl;
    Lbl("S0",  "────────────────────────",      x, y, thSep             ); y += sep;
    Lbl("Ti",  "Time   : " + tStr,             x, y, thText,      sz   ); y += s;
    Lbl("Md",  "Mode   : " + modeName,         x, y, modeClr,     sz   ); y += s;
    Lbl("Ft",  "Filter : " + filterStr,        x, y, filterClr,   sz   ); y += s;
    Lbl("FtV", filterValStr,                   x, y, filterValClr, sz  ); y += s;
    Lbl("Nw",  "News   : " + newsStr,          x, y, newsClr,     sz   ); y += s;
    Lbl("Gd",  "Guard  : " + guardStr,         x, y, guardClr,    sz   ); y += s;
    Lbl("Wk",  "Wknd   : " + wkndStr,          x, y, wkndClr,     sz   ); y += s;
    Lbl("S1",  "────────────────────────",      x, y, thSep             ); y += sep;
    Lbl("Bl",  StringFormat("Balance: $%.2f",        balance),     x, y, thText, sz); y += s;
    Lbl("Fl",  StringFormat("Float  : $%.2f  (%.2f%%)", totalProfit, pnlPct),
                                                                    x, y, cTotal, sz); y += s;
    Lbl("Tg",  StringFormat("Target : $%.2f",        InpTotalProfitTP),
                                                                    x, y, thText, sz); y += s;
    Lbl("Dy",  StringFormat("Daily  : $%.2f  (%.2f%%)", dailyPnl, dailyPnlPct),
                                                                    x, y, cDaily, sz); y += s;
    Lbl("S2",  "────────────────────────",      x, y, thSep             ); y += sep;
    Lbl("Bp",  StringFormat("BUY  : %d ord  Lot:%.2f  $%.2f", nBuy,  lotBuy,  buyProfit),
                                                                    x, y, cBuy,   sz); y += s;
    Lbl("Sp",  StringFormat("SELL : %d ord  Lot:%.2f  $%.2f", nSell, lotSell, sellProfit),
                                                                    x, y, cSell,  sz); y += s;
    Lbl("To",  StringFormat("Total: %d orders   Spread: %.0fpt", nBuy+nSell, spread),
                                                                    x, y, thText, sz);

    CreateBG("BG2", pX, bg2Y, pW, bg2H, thBg2, thBd2);

    y = bg2Y + 10;
    Lbl("P2T", "═══  ĐIỀU KHIỂN  ═══", cx, y, thP2T, sz);

    string themeLabel = g_darkMode ? "Light" : "Dark";
    CreateBtn("BtnTheme", themeLabel, pX + pW - 54, bg2Y + 7, 47, 18,
              thBtnThemeBg, thBtnThemeBd);

    ObjectDelete(0, GUI + "BtnModeAuto");
    ObjectDelete(0, GUI + "BtnModeSemi");
    ObjectDelete(0, GUI + "MdV");

    string toggleText = g_semiAutoMode ? ">> SEMI-AUTO" : ">> AUTO";
    color  toggleBg   = g_semiAutoMode ? C'100,50,0'   : C'0,80,25';
    color  toggleBd   = g_semiAutoMode ? C'220,130,40' : C'40,200,90';
    int    btnW       = pW - 16;
    CreateBtn("BtnToggleMode", toggleText, cx, bg2Y + 26, btnW, 22, toggleBg, toggleBd);

    string saStr = " ", saTotStr = " ";
    if(g_semiAutoMode)
    {
        saStr    = StringFormat("  TP: %dpip  SL: %dpip", InpSemiAutoTP, InpSemiAutoSL);
        saTotStr = StringFormat("  Tot TP:$%.1f  SL:$%.1f", InpSemiAutoTotalTP, InpSemiAutoTotalSL);
    }
    Lbl("P2SA",  saStr,    cx, bg2Y + 52, thSA1, sz);
    Lbl("P2SAT", saTotStr, cx, bg2Y + 52 + (g_semiAutoMode ? s : 0), thSA2, sz);

    int closeY = g_semiAutoMode ? bg2Y + 52 + s + s : bg2Y + 52;
    CreateBtn("BtnCloseAll", "  Close All Orders", cx, closeY, btnW, 22,
              thBtnCloseBg, thBtnCloseBd);

    ChartRedraw(0);
}

double NormalizePrice(double price) { return NormalizeDouble(price, _Digits); }

double NormalizeLot(double lot)
{
    lot = MathRound(lot / g_lotStep) * g_lotStep;
    lot = MathMax(lot, g_lotMin);
    lot = MathMin(lot, g_lotMax);
    return NormalizeDouble(lot, 2);
}
//+------------------------------------------------------------------+
