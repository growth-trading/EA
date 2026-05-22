//+------------------------------------------------------------------+
//|                                             DCA_Basket_EA.mq5    |
//|              DCA Basket 2 chiều - XAUUSD                         |
//|              Phân tích từ ảnh thực tế ngày 2026-05-19           |
//+------------------------------------------------------------------+
#property copyright "LongHD"
#property version   "1.11"
#property strict

#include <Trade\Trade.mqh>
#include <Trade\OrderInfo.mqh>
#include <Trade\PositionInfo.mqh>

//+------------------------------------------------------------------+
//| INPUTS                                                           |
//+------------------------------------------------------------------+

input group "========== 01. CORE =========="
input double  InpInitialLot       = 0.01;      // Lot ban đầu
input int     InpInitialDistance  = 300;       // Khoảng cách pending ban đầu, tính bằng point
input int     InpTrailingDistance = 300;       // Khoảng cách trailing pending ngược chiều, tính bằng point
input double  InpTotalProfitTP    = 5.0;       // Tổng lợi nhuận tiền để đóng tất cả lệnh
input int     InpMaxSpread        = 0;         // Spread tối đa, 0 = không lọc
input int     InpSlippage         = 30;        // Slippage/deviation

input group "========== 02. DCA AM - HE SO CONG =========="
input int     InpDCAMinDistance   = 300;       // Khoảng cách DCA âm tối thiểu so với lệnh gần nhất, tính bằng po
input double  InpDCALotAdd        = 0.01;      // Lot cộng thêm từ lệnh cùng chiều gần nhất
input int     InpMaxOrdersPerSide = 20;        // Số lệnh tối đa mỗi phía Buy/Sell
input int     InpMinRestDCA       = 1;         // Nghỉ tối thiểu giữa 2 lệnh DCA cùng phía (giây)

input group "========== 02B. DCA MULTIPLIER KHI AM TAI KHOAN =========="
input bool    InpDCAMultEnabled   = true;      // Bật hệ số nhân khi âm % tài khoản
input double  InpDCAMultTriggerPct= 5.0;       // Âm bao nhiêu % balance thì chuyển sang hệ số nhân
input double  InpDCAMultFactor    = 1.5;       // Hệ số nhân lot DCA khi kích hoạt
input double  InpDCAMaxLot        = 0.0;       // Giới hạn lot DCA tối đa, 0 = theo broker

input group "========== 03. OPTIONAL PROTECTION =========="
input double  InpPendingTP        = 0;         // TP riêng cho pending, 0 = tắt
input int     InpMinModifyPoints  = 1;         // Buộc tối thiểu để modify pending (điểm)
input bool    InpDebugLog         = true;      // In log để debug

input group "========== 04. FAST BASKET CLOSE =========="
input int     InpFastCloseLoops   = 8;         // Số vòng quét đóng nhanh khi đạt tổng tiền
input bool    InpAsyncClose       = true;      // true = gửi lệnh đóng async nhanh hơn
input int     InpTimerMS          = 250;       // Timer tiếp tục quét khi không có tick, 100-1000ms
input int     InpWaitNewCycleSec  = 5;         // Đợi x giây trước khi tạo chu kỳ pending mới

input group "========== 05. RED NEWS PAUSE - MT5 CALENDAR =========="
input bool    InpNewsPauseEnabled = true;      // Tạm dừng EA khi sắp có tin tức
input int     InpNewsPauseBefore  = 30;        // Dừng trước tin x phút
input int     InpNewsResumeAfter  = 30;        // Mở lại sau tin x phút
input bool    InpNewsFilterA      = true;      // Lọc tin độ phiên A
input string  InpNewsCurrA        = "JPY,CNY,AUD,NZD"; // Tiền tệ phiên A
input bool    InpNewsFilterAu     = true;      // Lọc tin độ phiên Au
input string  InpNewsCurrAu       = "EUR,GBP,CHF";     // Tiền tệ phiên Au
input bool    InpNewsFilterMy     = true;      // Lọc tin độ phiên Mỹ
input string  InpNewsCurrMy       = "USD,CAD";          // Tiền tệ phiên Mỹ
input int     InpNewsScanHours    = 24;        // Khoảng quét lịch tin sắp tới (giờ)
input int     InpNewsUpdateSec    = 60;        // Tần suất cập nhật lịch tin (giây)
input bool    InpDeletePendingNews= true;      // Xóa pending khi đang trong vùng tin

input group "========== 07. CAPITAL GUARD - LAI/LO THEO VON KHOI CHAY =========="
input bool    InpCapGuardProfit   = true;      // Dừng EA khi lãi x% vốn khởi chạy
input double  InpCapGuardProfitPct= 5.0;       // Lãi x% thì đóng sạch và tạm dừng
input bool    InpCapGuardLoss     = true;      // Dừng EA khi âm trạng thái x% vốn
input double  InpCapGuardLossPct  = 10.0;      // Âm x% vốn thì đóng sạch và tạm dừng
input bool    InpCapGuardResume   = true;      // Sau khi dừng %, đợi lệnh đầu tiên mới chạy lại
input bool    InpManageAllMagic0  = true;      // Quản lý cả lệnh tay magic 0 trên cùng symbol

//+------------------------------------------------------------------+
//| OBJECTS & STATE                                                  |
//+------------------------------------------------------------------+
CTrade        g_trade;
CPositionInfo g_pos;
COrderInfo    g_ord;

#define MAGIC_NUMBER 26051501

//--- Trading state
double   g_startCapital       = 0;
bool     g_capGuardActive     = false;
datetime g_lastDCATimeBuy     = 0;
datetime g_lastDCATimeSell    = 0;
datetime g_nextCycleAllowAt   = 0;

//--- News cache
struct NewsItem { datetime time; string currency; };
NewsItem  g_newsCache[];
datetime  g_lastNewsUpdate    = 0;
bool      g_inNewsZone        = false;

//+------------------------------------------------------------------+
//| INIT / DEINIT                                                    |
//+------------------------------------------------------------------+
int OnInit()
{
    g_trade.SetExpertMagicNumber(MAGIC_NUMBER);
    g_trade.SetDeviationInPoints(InpSlippage);
    g_trade.SetAsyncMode(InpAsyncClose);
    g_trade.SetTypeFilling(ORDER_FILLING_FOK);

    g_startCapital = AccountInfoDouble(ACCOUNT_BALANCE);

    EventSetMillisecondTimer(InpTimerMS);

    Log("EA v1.11 started | Magic=" + IntegerToString(MAGIC_NUMBER) +
        " | Capital=" + DoubleToString(g_startCapital, 2));

    return INIT_SUCCEEDED;
}

void OnDeinit(const int reason)
{
    EventKillTimer();
}

//+------------------------------------------------------------------+
void OnTick()  { RunEA(); }
void OnTimer() { RunEA(); }

//+------------------------------------------------------------------+
//| HÀM CHÍNH                                                        |
//+------------------------------------------------------------------+
void RunEA()
{
    // --- Capital Guard ---
    if(CheckCapitalGuard()) return;

    // --- Chờ sau basket close ---
    if(TimeCurrent() < g_nextCycleAllowAt) return;

    // --- Lọc spread ---
    if(!CheckSpread()) return;

    // --- Cập nhật và kiểm tra tin tức ---
    if(InpNewsPauseEnabled)
    {
        UpdateNewsCache();
        g_inNewsZone = IsInNewsZone();

        if(g_inNewsZone)
        {
            Log("PAUSED - NEWS ZONE");
            if(InpDeletePendingNews) DeleteAllPending();
            return;
        }
    }

    int buyPos   = CountPositions(POSITION_TYPE_BUY);
    int sellPos  = CountPositions(POSITION_TYPE_SELL);
    int buyPend  = CountPending(ORDER_TYPE_BUY_STOP);
    int sellPend = CountPending(ORDER_TYPE_SELL_STOP);
    int totalPos = buyPos + sellPos;

    // Không có gì → bắt đầu chu kỳ mới
    if(totalPos == 0 && buyPend == 0 && sellPend == 0)
    {
        PlaceInitialPending();
        return;
    }

    // Có vị thế → kiểm tra đóng basket + DCA + trailing
    if(totalPos > 0)
    {
        double totalProfit = GetTotalProfit();
        Log("Profit: " + DoubleToString(totalProfit, 2) + " USD"
            + " | B:" + IntegerToString(buyPos)
            + " S:" + IntegerToString(sellPos));

        if(totalProfit >= InpTotalProfitTP)
        {
            Log(">>> BASKET CLOSE! Profit=" + DoubleToString(totalProfit, 2));
            CloseAllOrders();
            g_nextCycleAllowAt = TimeCurrent() + InpWaitNewCycleSec;
            return;
        }

        if(buyPos > 0 && buyPos < InpMaxOrdersPerSide)
            ManageDCA(POSITION_TYPE_BUY);

        if(sellPos > 0 && sellPos < InpMaxOrdersPerSide)
            ManageDCA(POSITION_TYPE_SELL);

        TrailOppositePending(buyPos, sellPos, buyPend, sellPend);
    }
}

//+------------------------------------------------------------------+
//| Đặt 2 lệnh pending ban đầu                                       |
//+------------------------------------------------------------------+
void PlaceInitialPending()
{
    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);

    double buyPrice  = NormalizePrice(ask + InpInitialDistance * point);
    double sellPrice = NormalizePrice(bid - InpInitialDistance * point);

    double tpBuy = 0, tpSell = 0;
    if(InpPendingTP > 0)
    {
        tpBuy  = NormalizePrice(buyPrice  + InpPendingTP * point);
        tpSell = NormalizePrice(sellPrice - InpPendingTP * point);
    }

    bool ok1 = g_trade.BuyStop(InpInitialLot, buyPrice, _Symbol, 0, tpBuy,
                                ORDER_TIME_GTC, 0, "INIT_BUY_STOP");
    bool ok2 = g_trade.SellStop(InpInitialLot, sellPrice, _Symbol, 0, tpSell,
                                 ORDER_TIME_GTC, 0, "INIT_SELL_STOP");

    Log("New cycle | BuyStop=" + DoubleToString(buyPrice, _Digits) +
        " SellStop=" + DoubleToString(sellPrice, _Digits) +
        " Lot=" + DoubleToString(InpInitialLot, 2) +
        (ok1 && ok2 ? " [OK]" : " [FAIL]"));

}

//+------------------------------------------------------------------+
//| DCA khi lệnh thua lỗ                                             |
//+------------------------------------------------------------------+
void ManageDCA(ENUM_POSITION_TYPE dir)
{
    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);

    // Kiểm tra thời gian nghỉ
    datetime lastDCA = (dir == POSITION_TYPE_BUY) ? g_lastDCATimeBuy : g_lastDCATimeSell;
    if(TimeCurrent() - lastDCA < InpMinRestDCA) return;

    // Giá bất lợi nhất (lệnh DCA xa nhất)
    double extremePrice = GetExtremePositionPrice(dir);
    if(extremePrice <= 0) return;

    // Tính mức giá kích hoạt DCA tiếp theo
    double triggerPrice = (dir == POSITION_TYPE_BUY)
                          ? extremePrice - InpDCAMinDistance * point
                          : extremePrice + InpDCAMinDistance * point;

    bool triggered = (dir == POSITION_TYPE_BUY) ? (bid <= triggerPrice)
                                                 : (ask >= triggerPrice);
    if(!triggered) return;

    // Tính lot mới
    double lastLot    = GetExtremeLot(dir);
    double mult       = GetDCAMultiplier();
    double newLot     = NormalizeLot(lastLot + InpDCALotAdd * mult);
    if(InpDCAMaxLot > 0 && newLot > InpDCAMaxLot)
        newLot = NormalizeLot(InpDCAMaxLot);

    string comment = (dir == POSITION_TYPE_BUY) ? "DCA_BUY" : "DCA_SELL";
    bool ok = (dir == POSITION_TYPE_BUY) ? g_trade.Buy(newLot, _Symbol, 0, 0, 0, comment)
                                         : g_trade.Sell(newLot, _Symbol, 0, 0, 0, comment);
    if(ok)
    {
        if(dir == POSITION_TYPE_BUY) g_lastDCATimeBuy  = TimeCurrent();
        else                          g_lastDCATimeSell = TimeCurrent();
        Log("DCA " + (dir==POSITION_TYPE_BUY?"BUY":"SELL") +
            " Lot=" + DoubleToString(newLot, 2) +
            " Trigger=" + DoubleToString(triggerPrice, _Digits) +
            " Mult=" + DoubleToString(mult, 2));
    }
}

//+------------------------------------------------------------------+
//| Trail pending ngược chiều                                         |
//+------------------------------------------------------------------+
void TrailOppositePending(int buyPos, int sellPos, int buyPend, int sellPend)
{
    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);

    // BUY đang mở, chưa có SELL → trail Sell Stop lên
    if(buyPos > 0 && sellPos == 0 && sellPend > 0)
    {
        double newStop     = NormalizePrice(bid - InpTrailingDistance * point);
        double currentStop = GetPendingPrice(ORDER_TYPE_SELL_STOP);
        if(newStop > currentStop + InpMinModifyPoints * point)
        {
            ModifyPendingStop(ORDER_TYPE_SELL_STOP, newStop);
            Log("Trail SellStop → " + DoubleToString(newStop, _Digits));
        }
    }

    // SELL đang mở, chưa có BUY → trail Buy Stop xuống
    if(sellPos > 0 && buyPos == 0 && buyPend > 0)
    {
        double newStop     = NormalizePrice(ask + InpTrailingDistance * point);
        double currentStop = GetPendingPrice(ORDER_TYPE_BUY_STOP);
        if(newStop < currentStop - InpMinModifyPoints * point)
        {
            ModifyPendingStop(ORDER_TYPE_BUY_STOP, newStop);
            Log("Trail BuyStop → " + DoubleToString(newStop, _Digits));
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

//+------------------------------------------------------------------+
//| Đóng tất cả vị thế + xóa pending                                 |
//+------------------------------------------------------------------+
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
    Log("All orders closed.");
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

//+------------------------------------------------------------------+
//| NEWS PAUSE - MT5 Calendar                                        |
//+------------------------------------------------------------------+

// Cập nhật cache tin tức từ MT5 Calendar (chỉ tin đỏ - HIGH impact)
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

    // Xây danh sách tiền tệ cần lọc
    string currencies[];
    BuildCurrencyList(currencies);

    for(int i = 0; i < total; i++)
    {
        MqlCalendarEvent event;
        if(!CalendarEventById(values[i].event_id, event)) continue;

        // Chỉ lấy tin HIGH impact (màu đỏ)
        if(event.importance != CALENDAR_IMPORTANCE_HIGH) continue;

        MqlCalendarCountry country;
        if(!CalendarCountryById(event.country_id, country)) continue;

        // Lọc theo tiền tệ
        if(!IsCurrencyInList(country.currency, currencies)) continue;

        int idx = ArraySize(g_newsCache);
        ArrayResize(g_newsCache, idx + 1);
        g_newsCache[idx].time     = values[i].time;
        g_newsCache[idx].currency = country.currency;
    }

    Log("News cache updated: " + IntegerToString(ArraySize(g_newsCache)) + " events");
}

// Kiểm tra có đang trong vùng tin không
bool IsInNewsZone()
{
    datetime now          = TimeCurrent();
    datetime pauseBefore  = (datetime)(InpNewsPauseBefore * 60);
    datetime resumeAfter  = (datetime)(InpNewsResumeAfter * 60);

    for(int i = 0; i < ArraySize(g_newsCache); i++)
    {
        datetime newsTime = g_newsCache[i].time;
        if(now >= newsTime - pauseBefore && now <= newsTime + resumeAfter)
        {
            Log("News zone: " + g_newsCache[i].currency +
                " @ " + TimeToString(newsTime, TIME_DATE|TIME_MINUTES));
            return true;
        }
    }
    return false;
}

// Xây danh sách tiền tệ cần lọc từ các input string
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

//+------------------------------------------------------------------+
//| CAPITAL GUARD                                                    |
//+------------------------------------------------------------------+
bool CheckCapitalGuard()
{
    if(!InpCapGuardProfit && !InpCapGuardLoss) return false;

    if(g_capGuardActive)
    {
        if(InpCapGuardResume)
        {
            bool hasAny = (CountPositions(POSITION_TYPE_BUY) +
                           CountPositions(POSITION_TYPE_SELL) +
                           CountPending(ORDER_TYPE_BUY_STOP) +
                           CountPending(ORDER_TYPE_SELL_STOP)) > 0;
            if(!hasAny)
            {
                g_capGuardActive = false;
                g_startCapital   = AccountInfoDouble(ACCOUNT_BALANCE);
                Log("Capital Guard RESET. New base=" + DoubleToString(g_startCapital, 2));
            }
        }
        return g_capGuardActive;
    }

    double balance   = AccountInfoDouble(ACCOUNT_BALANCE);
    double equity    = AccountInfoDouble(ACCOUNT_EQUITY);
    if(g_startCapital <= 0) g_startCapital = balance;

    double changePct = (equity - g_startCapital) / g_startCapital * 100.0;

    if(InpCapGuardProfit && changePct >= InpCapGuardProfitPct)
    {
        Log("Capital Guard: PROFIT " + DoubleToString(changePct, 2) + "%");
        CloseAllOrders();
        g_capGuardActive = true;
        return true;
    }
    if(InpCapGuardLoss && changePct <= -InpCapGuardLossPct)
    {
        Log("Capital Guard: LOSS " + DoubleToString(changePct, 2) + "%");
        CloseAllOrders();
        g_capGuardActive = true;
        return true;
    }
    return false;
}

//+------------------------------------------------------------------+
//| HELPER FUNCTIONS                                                 |
//+------------------------------------------------------------------+
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

// Giá lệnh bất lợi nhất: Buy → thấp nhất | Sell → cao nhất
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
    if(InpManageAllMagic0 && magic == 0) return true;
    return false;
}

double NormalizePrice(double price) { return NormalizeDouble(price, _Digits); }

double NormalizeLot(double lot)
{
    double step = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
    lot = MathRound(lot / step) * step;
    lot = MathMax(lot, SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN));
    lot = MathMin(lot, SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX));
    return NormalizeDouble(lot, 2);
}

void Log(string msg) { if(InpDebugLog) Print("[DCA_Basket] ", msg); }
//+------------------------------------------------------------------+
