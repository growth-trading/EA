//+------------------------------------------------------------------+
//|                                          BOT_XAUUSD_v2.0.mq5     |
//|               Dual Straddle Breakout with Reset & Tiered         |
//|               Trailing (DSB-RTT) — XAUUSD Expert Advisor         |
//|               Client  : KINDLY - Hieu (Ha Noi)                   |
//|               Platform: MetaTrader 5 (MQL5)                      |
//|               Broker  : Exness Pro / Zero                        |
//|               Version : 2.0.0  |  Date: 22.04.2026               |
//+------------------------------------------------------------------+
#property strict
#property copyright "BOT_XAUUSD v2.0 — KINDLY"
#property version   "2.00"
#property description "Dual Straddle Breakout with Reset & Tiered Trailing (DSB-RTT)"

#include <Trade\Trade.mqh>
#include <Trade\PositionInfo.mqh>
#include <Trade\OrderInfo.mqh>
#include <Trade\SymbolInfo.mqh>

input group "=== GENERAL ==="
input int    InpMagicNumber    = 220426;          // Magic number nhận diện lệnh của bot
input string InpComment        = "BOT_XAUUSD_v2"; // Comment prefix cho tất cả order

input group "=== LOT SIZE ==="
input double InpLotSize        = 0.2;             // Lot cố định cho mỗi lệnh

input group "=== ENTRY ==="
input double InpStraddleDist   = 1.5;             // Khoảng cách straddle từ giá hiện tại (đơn vị giá)
input double InpStopLoss       = 3.0;             // SL cùng từ entry (đơn vị giá)

input group "=== TIME FILTER ==="
input int    InpStartHour      = 14;              // Giờ bắt đầu trading (giờ địa phương)
input int    InpEndHour        = 22;              // Giờ kết thúc T2-T5
input int    InpEndMinute      = 30;              // Phút kết thúc T2-T5 -> 22:30
input int    InpFridayEndHour  = 21;              // Giờ T6 đóng sớm
input int    InpFridayEndMin   = 0;               // Phút T6 đóng sớm -> 21:00
input int    InTimeGMT         = 0;               // Giờ địa phương

input group "=== ROLLOVER FILTER ==="
input int    InpRolloverStartH = 3;               // Giờ rollover bắt đầu
input int    InpRolloverStartM = 45;              // Phút rollover bắt đầu -> 03:45
input int    InpRolloverEndH   = 4;               // Giờ rollover kết thúc
input int    InpRolloverEndM   = 30;              // Phút rollover kết thúc -> 04:30

input group "=== ATR FILTER ==="
input int              InpATRPeriod = 14;         // Chu kỳ ATR
input ENUM_TIMEFRAMES  InpATRTF     = PERIOD_M5;  // Khung thời gian ATR
input double           InpATRMin    = 6.0;        // ATR min (đơn vị giá) để cho vào lệnh
input double           InpATRMax    = 28.0;       // ATR max (đơn vị giá) để cho vào lệnh

input group "=== ADX FILTER ==="
input int              InpADXPeriod = 14;         // Chu kỳ ADX
input ENUM_TIMEFRAMES  InpADXTF     = PERIOD_M15; // Khung thời gian ADX
input double           InpADXMin    = 20.0;       // ADX phải > giá trị này (exclusive)
input double           InpADXMax    = 55.0;       // ADX phải < giá trị này (exclusive)

input group "=== SPREAD FILTER ==="
input int    InpMaxSpreadPts   = 600;              // Spread max (điểm) = 0.6 giá

input group "=== RISK MANAGEMENT ==="
input double InpDailyLossPct   = 15.0;            // Giới hạn lỗ ngày (%)
input int    InpMaxLossStreak  = 5;               // Max thua liên tiếp trước khi pause
input int    InpPauseMinutes   = 60;              // Thời gian pause sau loss streak (phút)

input group "=== TRAILING TIERS ==="
input bool   InpEnableTrailing = true;   // Bật trailing SL theo tier
input double InpT1Trigger = 1.0;   // Tier 1 kích hoạt khi peak đạt (đơn vị giá)
input double InpT1Trail   = 0.5;   // Tier 1 cắt khi lùi bao nhiêu từ peak
input double InpT2Trigger = 1.5;   // Tier 2 kích hoạt khi peak đạt
input double InpT2Lock    = 1.0;   // Tier 2 lock SL tại mức lãi này (tính từ entry)
input double InpT3Trigger = 2.0;   // Tier 3 kích hoạt khi peak đạt
input double InpT3Lock    = 1.0;   // Tier 3 lock SL tại
input double InpT4Trigger = 3.0;   // Tier 4 kích hoạt khi peak đạt
input double InpT4Lock    = 2.0;   // Tier 4 lock SL tại
input double InpT5Trigger = 5.0;   // Tier 5 kích hoạt khi peak đạt
input double InpT5Lock    = 3.5;   // Tier 5 lock SL tại
input double InpT6Trigger = 8.0;   // Tier 6 kích hoạt khi peak đạt
input double InpT6Lock    = 90;    // Tier 6 lock tại X% lợi nhuận peak (0-100)

input group "=== TELEGRAM ==="
input bool   InpEnableTelegram = true;            // Master switch bật/tắt Telegram
input string InpTelegramToken  = "";              // Bot token từ @BotFather
input string InpTelegramChatID = "1903206789";    // Chat ID người nhận
input bool   InpNotifyEntry    = true;            // Notify khi đặt straddle
input bool   InpNotifyExit     = true;            // Notify khi đóng lệnh
input bool   InpNotifyReset    = true;            // Notify khi reset
input bool   InpNotifyDaily    = true;            // Gửi tổng kết ngày lúc 23:00

input group "=== PANEL HIỂN THỊ ==="
input int  InpPanelX   = 5;    // Vị trí X panel (pixel từ trái)
input int  InpPanelY   = 18;   // Vị trí Y panel (pixel từ trên)
input int  InpPanelW   = 262;  // Chiều rộng panel
input int  InpFontSz   = 9;    // Cỡ chữ
input int  InpLineH    = 16;   // Khoảng cách dòng (pixel)
input int  InpPanelGap = 4;    // Khoảng cách giữa các panel (pixel)
input int  InpPanel1H  = 0;    // Chiều cao panel 1 — Trạng thái (0 = tự tính)
input int  InpPanel2H  = 0;    // Chiều cao panel 2 — Bộ lọc (0 = tự tính)
input int  InpPanel3H  = 0;    // Chiều cao panel 3 — Cấu hình (0 = tự tính)
input int  InpPanel4H  = 0;    // Chiều cao panel 4 — Điều khiển (0 = tự tính)

//+------------------------------------------------------------------+
//|  CONSTANTS                                                       |
//+------------------------------------------------------------------+

#define EA_NAME              "BOT_XAUUSD_v2"
#define EA_VERSION           "2.0.0"
#define MAX_RETRY_COUNT      3
#define RETRY_DELAY_MS       500
#define TRAIL_RATE_LIMIT_S   1
#define STRADDLE_THROTTLE_S  30
#define MAX_STRADDLE_PER_MIN 3
#define MARGIN_SAFETY_FACTOR 2.5
#define EOD_BUFFER_MIN       15
#define DAILY_SUMMARY_HOUR   23
#define DAILY_SUMMARY_MIN    0
#define FORCE_CLOSE_PCT      20.0
#define ORPHAN_CHECK_S       3
#define DISCONNECT_ALERT_S   300

//+------------------------------------------------------------------+
//|  STATE MACHINE                                                   |
//+------------------------------------------------------------------+

enum EA_STATE {
    STATE_IDLE,         // Không có position/pending, chờ filter PASS
    STATE_PLACING,      // Đang đặt straddle
    STATE_WAITING,      // Cả 2 pending đang treo, chờ fill
    STATE_POSITION,     // Có ít nhất 1 position đang active
    STATE_RECOVERING,   // Vừa đóng lỗ, đang check điều kiện reset
    STATE_PAUSED,       // Loss streak 5, chờ 60 phút
    STATE_DAILY_STOP,   // Chạm 15% lỗ ngày, dừng đến nửa đêm
    STATE_EOD_CLOSE,    // Đến giờ EOD, đóng tất cả
    STATE_DISCONNECTED  // Mất kết nối broker
};

//+------------------------------------------------------------------+
//|  GLOBALS                                                         |
//+------------------------------------------------------------------+

EA_STATE g_state             = STATE_IDLE;
EA_STATE g_prevState         = STATE_IDLE;

double   g_dayStartEquity    = 0.0;
double   g_todayProfit       = 0.0;
bool     g_dailyLimitHit     = false;
int      g_lossStreak        = 0;
datetime g_pauseUntil        = 0;
datetime g_lastDayChecked    = 0;

datetime g_lastStraddleTime  = 0;
int      g_straddleInMin     = 0;
datetime g_straddleMinStart  = 0;

ulong    g_pendingBuyTkt     = 0;
ulong    g_pendingSellTkt    = 0;
double   g_pendingBuyPx      = 0.0;
double   g_pendingSellPx     = 0.0;
datetime g_lastFillTime      = 0;
ulong    g_filledPosId       = 0;

datetime g_lastTrailTime     = 0;
datetime g_lastResetTime     = 0;

int      g_handleATR         = INVALID_HANDLE;
int      g_handleADX         = INVALID_HANDLE;

int      g_totalTrades       = 0;
int      g_totalWins         = 0;
int      g_totalLosses       = 0;
double   g_grossProfit       = 0.0;
double   g_grossLoss         = 0.0;
double   g_maxDDIntraday     = 0.0;
double   g_peakEquityToday   = 0.0;
long     g_rejTime           = 0;
long     g_rejRoll           = 0;
long     g_rejSpread         = 0;
long     g_rejATR            = 0;
long     g_rejADX            = 0;

bool     g_dailySummarySent  = false;
datetime g_disconnectStart   = 0;

#define MAX_TRACKED 30
ulong    g_peakTkt[MAX_TRACKED];
double   g_peakPnl[MAX_TRACKED];
int      g_peakCnt = 0;

CTrade          g_trade;
CPositionInfo   g_posInfo;
COrderInfo      g_orderInfo;
CSymbolInfo     g_symInfo;

const string GUI = "BOT_";

//+------------------------------------------------------------------+
//|  HELPERS                                                         |
//+------------------------------------------------------------------+

double PriceToPoints(double price_value) { return price_value / _Point; }
double PointsToPrice(double points_value) { return points_value * _Point; }

double GetBrokerStopLevelPrice() {
    return PointsToPrice((double)SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL));
}

//+------------------------------------------------------------------+
//|  PEAK TRACKING                                                   |
//+------------------------------------------------------------------+

int PeakIdx(ulong tkt) {
    for(int i = 0; i < g_peakCnt; i++)
        if(g_peakTkt[i] == tkt) return i;
    return -1;
}

double PeakGet(ulong tkt) {
    int i = PeakIdx(tkt);
    return (i >= 0) ? g_peakPnl[i] : 0.0;
}

void PeakSet(ulong tkt, double val) {
    int i = PeakIdx(tkt);
    if(i >= 0) { g_peakPnl[i] = val; return; }
    if(g_peakCnt < MAX_TRACKED) {
        g_peakTkt[g_peakCnt] = tkt;
        g_peakPnl[g_peakCnt] = val;
        g_peakCnt++;
    }
}

void PeakRemove(ulong tkt) {
    int i = PeakIdx(tkt);
    if(i < 0) return;
    g_peakCnt--;
    g_peakTkt[i] = g_peakTkt[g_peakCnt];
    g_peakPnl[i] = g_peakPnl[g_peakCnt];
}

//+------------------------------------------------------------------+
//|  CALC LOCK LEVEL                                                 |
//+------------------------------------------------------------------+

double CalcLockLevel(double peak) {
    // T6 dùng % peak thay vì fixed lock — càng lên cao càng bảo vệ nhiều
    if(InpT6Trigger > 0 && peak >= InpT6Trigger) return peak * InpT6Lock / 100.0;
    if(InpT5Trigger > 0 && peak >= InpT5Trigger) return InpT5Lock;
    if(InpT4Trigger > 0 && peak >= InpT4Trigger) return InpT4Lock;
    if(InpT3Trigger > 0 && peak >= InpT3Trigger) return InpT3Lock;
    if(InpT2Trigger > 0 && peak >= InpT2Trigger) return InpT2Lock;
    if(InpT1Trigger > 0 && peak >= InpT1Trigger) return peak - InpT1Trail; // dynamic trail
    return -1.0;
}

//+------------------------------------------------------------------+
//|  TELEGRAM                                                        |
//+------------------------------------------------------------------+

string UrlEncode(string text) {
    string result = "";
    uchar  utf8[];
    StringToCharArray(text, utf8, 0, WHOLE_ARRAY, CP_UTF8);
    int uLen = ArraySize(utf8);
    for(int i = 0; i < uLen - 1; i++) {  // bỏ null terminator cuối
        uchar c = utf8[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
            result += CharToString(c);
        else if(c == ' ')
            result += "+";
        else
            result += StringFormat("%%%02X", (int)c);
    }
    return result;
}

datetime g_lastTelegramTime = 0;

bool SendTelegram(string text) {
    if(!InpEnableTelegram) return false;
    if(StringLen(InpTelegramToken) < 20) return false;
    if(TimeCurrent() - g_lastTelegramTime < 1) Sleep(1000); // rate limit 1 msg/giây
    string url     = "https://api.telegram.org/bot" + InpTelegramToken + "/sendMessage";
    string headers = "Content-Type: application/x-www-form-urlencoded\r\n";
    string body    = "chat_id=" + InpTelegramChatID + "&text=" + UrlEncode(text) + "&parse_mode=Markdown";
    char   post[], result[];
    StringToCharArray(body, post, 0, StringLen(body));
    string resHeaders;
    int httpCode = WebRequest("POST", url, headers, 5000, post, result, resHeaders);
    g_lastTelegramTime = TimeCurrent();
    if(httpCode == -1)  { GetLastError(); return false; }
    if(httpCode == 429) { Sleep(1000); return SendTelegram(text); }
    return (httpCode == 200);
}

void NotifyStart() {
    SendTelegram(StringFormat(
        "KHỞI ĐỘNG: %s v%s\nSymbol: %s | Lot: %.2f | SL: %.1f | Straddle: %.1f\n"
        "Trailing T1: đạt %.1f -> trail %.1f\n"
        "T2: %.1f->%.1f | T3: %.1f->%.1f\n"
        "T4: %.1f->%.1f | T5: %.1f->%.1f | T6: đạt %.1f -> %.0f%% peak",
        EA_NAME, EA_VERSION, _Symbol, InpLotSize, InpStopLoss, InpStraddleDist,
        InpT1Trigger, InpT1Trail,
        InpT2Trigger, InpT2Lock, InpT3Trigger, InpT3Lock,
        InpT4Trigger, InpT4Lock, InpT5Trigger, InpT5Lock,
        InpT6Trigger, InpT6Lock));
}

void NotifyStop(string reason)                  { SendTelegram(StringFormat("DỪNG: %s\nLý do: %s", EA_NAME, reason)); }
void NotifyStraddleSetup(double b, double s)    { if(!InpNotifyEntry)  return; SendTelegram(StringFormat("STRADDLE ĐẶT OK\nBuy Stop: %.2f | Sell Stop: %.2f\nSL: %.1f | Trailing 6-tier", b, s, InpStopLoss)); }
void NotifyCloseTP(ulong id, double p)          { if(!InpNotifyExit)   return; SendTelegram(StringFormat("ĐÓNG LÃI - Pos #%llu\nLãi: +%.2f USD", id, p)); }
void NotifyCloseSL(ulong id, double p)          { if(!InpNotifyExit)   return; SendTelegram(StringFormat("SL HIT - Pos #%llu\nLỗ: %.2f USD", id, p)); }
void NotifyResetSetup(double b, double s)       { if(!InpNotifyReset)  return; SendTelegram(StringFormat("RESET Straddle\nBuy Stop: %.2f | Sell Stop: %.2f", b, s)); }
void NotifyPause(int streak)                    { SendTelegram(StringFormat("EA TẠM DỪNG\n%d lệnh thua liên tiếp -> pause %d phút", streak, InpPauseMinutes)); }
void NotifyResume()                             { SendTelegram("EA TIẾP TỤC - Hết thời gian pause"); }
void NotifyDailyLimitHit(double pct)            { SendTelegram(StringFormat("DAILY LIMIT HIT\nĐã lỗ %.1f%% equity ngày -> EA dừng đến nửa đêm", pct)); }
void NotifyErrorAlert(string msg)               { SendTelegram("LỖI NGHIÊM TRỌNG\n" + msg); }

void NotifyDailySummary() {
    if(!InpNotifyDaily) return;
    double wr  = (g_totalTrades > 0) ? (100.0 * g_totalWins / g_totalTrades) : 0.0;
    double pf  = (g_grossLoss  != 0) ? (g_grossProfit / MathAbs(g_grossLoss)) : 0.0;
    double net = g_grossProfit + g_grossLoss;
    SendTelegram(StringFormat(
        "TỔNG KẾT NGÀY\nTrades: %d | W: %d | L: %d\nWin Rate: %.1f%% | PF: %.2f\nNet PL: %.2f USD\nMax DD: %.2f USD\nFilter rejects: T=%lld R=%lld S=%lld ATR=%lld ADX=%lld",
        g_totalTrades, g_totalWins, g_totalLosses, wr, pf, net, g_maxDDIntraday,
        g_rejTime, g_rejRoll, g_rejSpread, g_rejATR, g_rejADX));
}

//+------------------------------------------------------------------+
//|  FILTERS                                                         |
//+------------------------------------------------------------------+

bool IsInTradingHour() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    if(now.day_of_week == 0 || now.day_of_week == 6) return false;
    if(now.hour < (InpStartHour - InTimeGMT)) return false;
    if(now.day_of_week == 5) {
        if(now.hour > (InpFridayEndHour - InTimeGMT)) return false;
        if(now.hour == (InpFridayEndHour - InTimeGMT) && now.min >= InpFridayEndMin) return false;
    } else {
        if(now.hour > (InpEndHour - InTimeGMT)) return false;
        if(now.hour == (InpEndHour - InTimeGMT) && now.min >= InpEndMinute) return false;
    }
    return true;
}

bool IsInRolloverWindow() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    int curMin   = now.hour * 60 + now.min;
    int startMin = (InpRolloverStartH - InTimeGMT) * 60 + InpRolloverStartM;
    int endMin   = (InpRolloverEndH   - InTimeGMT) * 60 + InpRolloverEndM;
    return (curMin >= startMin && curMin <= endMin);
}

bool IsSpreadOK()   { return (int)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD) <= InpMaxSpreadPts; }

double GetATR() { double b[]; return (CopyBuffer(g_handleATR, 0, 0, 1, b) == 1) ? b[0] : 0.0; }
double GetADX() { double b[]; return (CopyBuffer(g_handleADX, 0, 0, 1, b) == 1) ? b[0] : 0.0; }  // buffer 0 = ADX main line

bool IsATROK() { double v = GetATR(); return (v > 0 && v >= InpATRMin && v <= InpATRMax); }
bool IsADXOK() { double v = GetADX(); return (v > 0 && v > InpADXMin && v < InpADXMax); }  // exclusive bounds

bool AllFiltersPass() {
    if(!IsInTradingHour())   { g_rejTime++;   return false; }
    if(IsInRolloverWindow()) { g_rejRoll++;   return false; }
    if(!IsSpreadOK())        { g_rejSpread++; return false; }
    if(!IsATROK())           { g_rejATR++;    return false; }
    if(!IsADXOK())           { g_rejADX++;    return false; }
    return true;
}

//+------------------------------------------------------------------+
//|  TRAILING STOP                                                   |
//+------------------------------------------------------------------+

void ApplyTrailing(ulong ticket) {
    if(!InpEnableTrailing) return;
    if(!g_posInfo.SelectByTicket(ticket)) return;
    if(g_posInfo.Magic() != InpMagicNumber) return;
    if(g_posInfo.Symbol() != _Symbol) return;

    double entry = g_posInfo.PriceOpen();
    double curSL = g_posInfo.StopLoss();
    ENUM_POSITION_TYPE pType = g_posInfo.PositionType();

    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double mktPx = (pType == POSITION_TYPE_BUY) ? bid : ask;
    double profit = (pType == POSITION_TYPE_BUY) ? (mktPx - entry) : (entry - mktPx);

    double peak = PeakGet(ticket);
    if(profit > peak) { peak = profit; PeakSet(ticket, peak); }

    double lockLevel = CalcLockLevel(peak);
    if(lockLevel <= 0.0) return;

    double newSL = (pType == POSITION_TYPE_BUY)
                    ? NormalizeDouble(entry + lockLevel, _Digits)
                    : NormalizeDouble(entry - lockLevel, _Digits);

    // PositionModify(sl=0) xóa SL trên MT5 — guard bắt buộc
    if(newSL <= 0.0) return;

    // Tránh broker reject khi SL quá gần giá (stop level requirement)
    double stopDist = GetBrokerStopLevelPrice();
    if(stopDist > 0) {
        if(pType == POSITION_TYPE_BUY  && bid - newSL < stopDist) return;
        if(pType == POSITION_TYPE_SELL && newSL - ask < stopDist) return;
    }

    // curSL == 0 → chưa có SL, luôn đặt mới
    // SELL: điều kiện newSL < curSL sẽ fail khi curSL=0 vì newSL > 0 luôn
    bool shouldModify = false;
    if(pType == POSITION_TYPE_BUY)
        shouldModify = (curSL == 0 || newSL > curSL + _Point);
    else
        shouldModify = (curSL == 0 || newSL < curSL - _Point);
    if(!shouldModify) return;

    if(TimeCurrent() - g_lastTrailTime < TRAIL_RATE_LIMIT_S) return;
    if(g_trade.PositionModify(ticket, newSL, 0))
        g_lastTrailTime = TimeCurrent();
}

void ManageTrailing() {
    for(int i = PositionsTotal() - 1; i >= 0; i--) {
        if(g_posInfo.SelectByIndex(i) &&
           g_posInfo.Magic() == InpMagicNumber &&
           g_posInfo.Symbol() == _Symbol)
            ApplyTrailing(g_posInfo.Ticket());
    }
}

//+------------------------------------------------------------------+
//|  COUNT HELPERS                                                   |
//+------------------------------------------------------------------+

int CountMyPositions() {
    int cnt = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--)
        if(g_posInfo.SelectByIndex(i) && g_posInfo.Magic() == InpMagicNumber && g_posInfo.Symbol() == _Symbol)
            cnt++;
    return cnt;
}

int CountMyPendingOrders() {
    int cnt = 0;
    for(int i = OrdersTotal() - 1; i >= 0; i--)
        if(g_orderInfo.SelectByIndex(i) && g_orderInfo.Magic() == InpMagicNumber && g_orderInfo.Symbol() == _Symbol)
            cnt++;
    return cnt;
}

//+------------------------------------------------------------------+
//|  ORDER OPERATIONS                                                |
//+------------------------------------------------------------------+

void CloseAllPositions(string reason) {
    for(int i = PositionsTotal() - 1; i >= 0; i--) {
        if(g_posInfo.SelectByIndex(i) && g_posInfo.Magic() == InpMagicNumber && g_posInfo.Symbol() == _Symbol)
            g_trade.PositionClose(g_posInfo.Ticket());
    }
}

void CancelAllPendingOrders() {
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(g_orderInfo.SelectByIndex(i) && g_orderInfo.Magic() == InpMagicNumber && g_orderInfo.Symbol() == _Symbol)
            g_trade.OrderDelete(g_orderInfo.Ticket());
    }
}

bool CancelOrderSafe(ulong ticket) {
    for(int attempt = 0; attempt < 2; attempt++) {
        if(g_trade.OrderDelete(ticket)) return true;
        Sleep(RETRY_DELAY_MS);
    }
    NotifyErrorAlert(StringFormat("ORPHAN — Không hủy được order #%llu!", ticket));
    return false;
}

//+------------------------------------------------------------------+
//|  MARGIN / ERROR HANDLING                                         |
//+------------------------------------------------------------------+

bool HasSufficientMargin() {
    double mgnReq = 0.0;
    if(!OrderCalcMargin(ORDER_TYPE_BUY, _Symbol, InpLotSize,
                        SymbolInfoDouble(_Symbol, SYMBOL_ASK), mgnReq)) return false;
    return AccountInfoDouble(ACCOUNT_MARGIN_FREE) >= mgnReq * MARGIN_SAFETY_FACTOR;
}

bool HandleRetcode(uint retcode, string ctx) {
    switch(retcode) {
        case TRADE_RETCODE_DONE:
        case TRADE_RETCODE_PLACED:        return true;
        case TRADE_RETCODE_REQUOTE:
        case TRADE_RETCODE_PRICE_CHANGED: Sleep(RETRY_DELAY_MS); return false;
        case TRADE_RETCODE_NO_MONEY:      NotifyErrorAlert(ctx + ": Tài khoản không đủ margin!"); return false;
        default:                          return false;
    }
}

//+------------------------------------------------------------------+
//|  PLACE STRADDLE                                                  |
//+------------------------------------------------------------------+

bool PlaceStraddle(bool isReset, double resetAtPrice) {
    if(!isReset) {
        if(TimeCurrent() - g_lastStraddleTime < STRADDLE_THROTTLE_S) return false;
        if(TimeCurrent() - g_straddleMinStart < 60) {
            g_straddleInMin++;
            if(g_straddleInMin > MAX_STRADDLE_PER_MIN) {
                NotifyErrorAlert("Phát hiện spam straddle — tự động pause");
                g_pauseUntil = TimeCurrent() + InpPauseMinutes * 60;
                g_state      = STATE_PAUSED;
                CancelAllPendingOrders();
                return false;
            }
        } else {
            g_straddleInMin    = 1;
            g_straddleMinStart = TimeCurrent();
        }
    }
    g_lastStraddleTime = TimeCurrent();

    if(!g_symInfo.RefreshRates()) return false;
    double bid = g_symInfo.Bid();
    double ask = g_symInfo.Ask();
    if(bid <= 0 || ask <= 0 || ask <= bid) return false;

    double stopLvlPx = GetBrokerStopLevelPrice();
    double minDist   = stopLvlPx * 1.1;  // +10% buffer
    double buyPx, sellPx;

    if(isReset && resetAtPrice > 0.0) {
        buyPx  = resetAtPrice + InpStraddleDist;
        sellPx = resetAtPrice - InpStraddleDist;
        // Fallback: nếu giá SL quá xa thị trường
        if(MathAbs(buyPx - ask) < stopLvlPx || MathAbs(sellPx - bid) < stopLvlPx) {
            buyPx  = ask + InpStraddleDist;
            sellPx = bid - InpStraddleDist;
        }
    } else {
        buyPx  = ask + InpStraddleDist;
        sellPx = bid - InpStraddleDist;
    }

    if(buyPx - ask  < minDist) buyPx  = NormalizeDouble(ask + minDist, _Digits);
    if(bid - sellPx < minDist) sellPx = NormalizeDouble(bid - minDist, _Digits);
    buyPx  = NormalizeDouble(buyPx,  _Digits);
    sellPx = NormalizeDouble(sellPx, _Digits);

    // TP = 0: quản lý hoàn toàn bởi trailing tier
    double sl_buy  = NormalizeDouble(buyPx  - InpStopLoss, _Digits);
    double sl_sell = NormalizeDouble(sellPx + InpStopLoss, _Digits);
    string sfx     = isReset ? "_R" : "";

    g_trade.SetExpertMagicNumber(InpMagicNumber);
    g_trade.SetDeviationInPoints(20);
    g_trade.SetTypeFilling(ORDER_FILLING_RETURN);

    bool  buyOK = false; ulong buyTkt = 0;
    for(int att = 0; att < MAX_RETRY_COUNT; att++) {
        buyOK = g_trade.BuyStop(InpLotSize, buyPx, _Symbol, sl_buy, 0,
                                ORDER_TIME_GTC, 0, InpComment + "_BUY" + sfx);
        if(buyOK) { buyTkt = g_trade.ResultOrder(); break; }
        if(!HandleRetcode(g_trade.ResultRetcode(), "BuyStop")) break;
    }
    if(!buyOK || buyTkt == 0) return false;

    bool  sellOK = false; ulong sellTkt = 0;
    for(int att = 0; att < MAX_RETRY_COUNT; att++) {
        sellOK = g_trade.SellStop(InpLotSize, sellPx, _Symbol, sl_sell, 0,
                                  ORDER_TIME_GTC, 0, InpComment + "_SELL" + sfx);
        if(sellOK) { sellTkt = g_trade.ResultOrder(); break; }
        if(!HandleRetcode(g_trade.ResultRetcode(), "SellStop")) break;
    }

    // ATOMICITY: nếu SellStop thất bại, rollback BuyStop
    if(!sellOK || sellTkt == 0) {
        if(!CancelOrderSafe(buyTkt))
            NotifyErrorAlert(StringFormat("ORPHAN BuyStop #%llu — cần kiểm tra thủ công!", buyTkt));
        return false;
    }

    g_pendingBuyTkt  = buyTkt;  g_pendingSellTkt = sellTkt;
    g_pendingBuyPx   = buyPx;   g_pendingSellPx  = sellPx;

    if(isReset) NotifyResetSetup(buyPx, sellPx);
    else        NotifyStraddleSetup(buyPx, sellPx);

    g_state = STATE_WAITING;
    return true;
}

//+------------------------------------------------------------------+
//|  RESET                                                           |
//+------------------------------------------------------------------+

void TryReset(double slHitPrice) {
    if(g_lossStreak >= InpMaxLossStreak)                     return;
    if(g_dailyLimitHit)                                      return;
    if(!IsInTradingHour())                                   return;
    if(!AllFiltersPass())                                    return;
    if(TimeCurrent() - g_lastResetTime <= 1)                 return;
    if(CountMyPositions() > 0 || CountMyPendingOrders() > 0) return;
    g_lastResetTime = TimeCurrent();
    PlaceStraddle(true, slHitPrice);
}

//+------------------------------------------------------------------+
//|  RISK MANAGEMENT                                                 |
//+------------------------------------------------------------------+

void CheckNewDay() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    datetime today = StringToTime(StringFormat("%04d.%02d.%02d", now.year, now.mon, now.day));
    if(today == g_lastDayChecked) return;
    g_lastDayChecked   = today;
    g_dayStartEquity   = AccountInfoDouble(ACCOUNT_EQUITY);
    g_todayProfit      = 0.0;    g_dailyLimitHit    = false;
    g_totalTrades      = 0;      g_totalWins        = 0;
    g_totalLosses      = 0;      g_grossProfit      = 0.0;
    g_grossLoss        = 0.0;    g_maxDDIntraday    = 0.0;
    g_peakEquityToday  = g_dayStartEquity;
    g_dailySummarySent = false;
    g_rejTime = g_rejRoll = g_rejSpread = g_rejATR = g_rejADX = 0;
}

void UpdateIntradayDD() {
    double eq = AccountInfoDouble(ACCOUNT_EQUITY);
    if(eq > g_peakEquityToday) g_peakEquityToday = eq;
    double dd = g_peakEquityToday - eq;
    if(dd > g_maxDDIntraday) g_maxDDIntraday = dd;
}

void CheckDailyLossLimit() {
    if(g_dailyLimitHit || g_dayStartEquity <= 0) return;
    double eq        = AccountInfoDouble(ACCOUNT_EQUITY);
    double totalDDPct = (g_dayStartEquity - eq) / g_dayStartEquity * 100.0;
    if(totalDDPct >= FORCE_CLOSE_PCT) { CloseAllPositions("ForceClose DD>20%"); CancelAllPendingOrders(); }
    if(g_todayProfit >= 0) return;
    double lossPct = (-g_todayProfit / g_dayStartEquity) * 100.0;
    if(lossPct >= InpDailyLossPct) {
        g_dailyLimitHit = true;
        g_state         = STATE_DAILY_STOP;
        CloseAllPositions("DailyLimit");
        CancelAllPendingOrders();
        NotifyDailyLimitHit(lossPct);
    }
}

void UpdateAfterTrade(double closedPL) {
    g_totalTrades++;
    if(closedPL > 0) {
        g_totalWins++;  g_grossProfit += closedPL; g_todayProfit += closedPL; g_lossStreak = 0;
    } else {
        g_totalLosses++; g_grossLoss += closedPL; g_todayProfit += closedPL; g_lossStreak++;
        if(g_lossStreak >= InpMaxLossStreak) {
            g_pauseUntil = TimeCurrent() + (datetime)(InpPauseMinutes * 60);
            g_state      = STATE_PAUSED;
            CancelAllPendingOrders();
            NotifyPause(g_lossStreak);
        }
    }
    CheckDailyLossLimit();
}

void CheckPauseExpiry() {
    if(g_state != STATE_PAUSED) return;
    if(g_pauseUntil > 0 && TimeCurrent() >= g_pauseUntil) {
        g_pauseUntil = 0; g_state = STATE_IDLE; NotifyResume();
    }
}

bool IsEODClose() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    if(now.day_of_week == 0 || now.day_of_week == 6) return true;
    int cur = now.hour * 60 + now.min;
    int eod = (now.day_of_week == 5)
                ? ((InpFridayEndHour - InTimeGMT) * 60 + InpFridayEndMin)
                : ((InpEndHour       - InTimeGMT) * 60 + InpEndMinute);
    return (cur >= eod);
}

//+------------------------------------------------------------------+
//|  SPECIAL CASES                                                   |
//+------------------------------------------------------------------+

// Sau ORPHAN_CHECK_S giây từ fill, hủy leg còn lại nếu không fill theo
void CheckOrphanOrder() {
    if(g_lastFillTime == 0) return;
    if(TimeCurrent() - g_lastFillTime < (datetime)ORPHAN_CHECK_S) return;
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(g_orderInfo.SelectByIndex(i) && g_orderInfo.Magic() == InpMagicNumber && g_orderInfo.Symbol() == _Symbol)
            CancelOrderSafe(g_orderInfo.Ticket());
    }
    g_lastFillTime = 0;
    g_filledPosId  = 0;
}

bool ValidateTick() {
    if(!g_symInfo.RefreshRates()) return false;
    if(g_symInfo.Bid() == 0.0 || g_symInfo.Ask() == 0.0) return false;
    return (g_symInfo.Ask() > g_symInfo.Bid());
}

//+------------------------------------------------------------------+
//|  VALIDATION                                                      |
//+------------------------------------------------------------------+

bool ValidateInputs() {
    bool ok = true;
    double volMin = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
    double volMax = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
    if(InpLotSize < volMin || InpLotSize > volMax)
        { Print("INPUT ERROR: InpLotSize=", InpLotSize, " phải trong [", volMin, ", ", volMax, "]"); ok = false; }
    if(InpStopLoss <= 0)
        { Print("INPUT ERROR: InpStopLoss phải > 0"); ok = false; }
    if(InpStraddleDist <= 0)
        { Print("INPUT ERROR: InpStraddleDist phải > 0"); ok = false; }
    if(InpT1Trigger <= 0 || InpT1Trail <= 0 || InpT1Trail >= InpT1Trigger)
        { Print("INPUT ERROR: T1 — Trigger phải > 0, Trail phải > 0 và < Trigger"); ok = false; }
    if(InpT2Trigger > 0 && InpT2Trigger <= InpT1Trigger) { Print("INPUT ERROR: T2Trigger phải > T1Trigger"); ok = false; }
    if(InpT3Trigger > 0 && InpT3Trigger <= InpT2Trigger) { Print("INPUT ERROR: T3Trigger phải > T2Trigger"); ok = false; }
    if(InpT4Trigger > 0 && InpT4Trigger <= InpT3Trigger) { Print("INPUT ERROR: T4Trigger phải > T3Trigger"); ok = false; }
    if(InpT5Trigger > 0 && InpT5Trigger <= InpT4Trigger) { Print("INPUT ERROR: T5Trigger phải > T4Trigger"); ok = false; }
    if(InpT6Trigger > 0 && InpT6Trigger <= InpT5Trigger) { Print("INPUT ERROR: T6Trigger phải > T5Trigger"); ok = false; }
    if(InpT6Lock <= 0 || InpT6Lock > 100)                { Print("INPUT ERROR: T6Lock phải trong (0, 100]"); ok = false; }
    if(InpATRMin >= InpATRMax)                            { Print("INPUT ERROR: InpATRMin phải < InpATRMax"); ok = false; }
    if(InpADXMin >= InpADXMax || InpADXMin < 0 || InpADXMax > 100) { Print("INPUT ERROR: ADX range không hợp lệ"); ok = false; }
    if(InpDailyLossPct <= 0 || InpDailyLossPct >= 100)   { Print("INPUT ERROR: InpDailyLossPct phải trong (0, 100)"); ok = false; }
    if(InpStartHour >= InpEndHour || InpStartHour < 0 || InpEndHour > 23) { Print("INPUT ERROR: StartHour/EndHour không hợp lệ"); ok = false; }
    if(InpEnableTelegram && StringLen(InpTelegramToken) < 40) Print("INPUT WARN: Token có vẻ không đủ dài (< 40 ký tự)");
    return ok;
}

void ValidateAccountType() {
    long mode = AccountInfoInteger(ACCOUNT_TRADE_MODE);
    if(mode == ACCOUNT_TRADE_MODE_CONTEST)     Print("CONTEST account");
    else if(mode == ACCOUNT_TRADE_MODE_DEMO)   Print("DEMO account");
    else                                       Print("REAL account");
}

//+------------------------------------------------------------------+
//|  STATE RECOVERY                                                  |
//+------------------------------------------------------------------+

void RecoverState() {
    int posCnt = CountMyPositions();
    int ordCnt = CountMyPendingOrders();
    if(posCnt > 0)      g_state = STATE_POSITION;
    else if(ordCnt > 0) g_state = STATE_WAITING;
    else                g_state = STATE_IDLE;

    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    datetime dayStart = StringToTime(StringFormat("%04d.%02d.%02d", now.year, now.mon, now.day));
    if(HistorySelect(dayStart, TimeCurrent())) {
        int streak = 0;
        int total  = HistoryDealsTotal();
        for(int i = total - 1; i >= 0; i--) {
            ulong dt = HistoryDealGetTicket(i);
            if(HistoryDealGetInteger(dt, DEAL_MAGIC)  != InpMagicNumber) continue;
            if(HistoryDealGetString(dt,  DEAL_SYMBOL) != _Symbol) continue;
            if((ENUM_DEAL_ENTRY)HistoryDealGetInteger(dt, DEAL_ENTRY) != DEAL_ENTRY_OUT) continue;
            if(HistoryDealGetDouble(dt, DEAL_PROFIT) < 0) streak++;
            else { streak = 0; break; }  // thắng cuối → reset streak
        }
        g_lossStreak = streak;
    } else {
        g_lossStreak = 0;
    }
}

void CheckDailySummary() {
    if(g_dailySummarySent) return;
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    if(now.hour == DAILY_SUMMARY_HOUR && now.min == DAILY_SUMMARY_MIN) {
        NotifyDailySummary();
        g_dailySummarySent = true;
    }
}

//+------------------------------------------------------------------+
//|  EVENT HANDLERS                                                  |
//+------------------------------------------------------------------+

int OnInit() {
    if(!ValidateInputs()) return INIT_PARAMETERS_INCORRECT;
    ValidateAccountType();
    if(!g_symInfo.Name(_Symbol)) { Print("Lỗi khởi tạo CSymbolInfo cho: ", _Symbol); return INIT_FAILED; }

    g_trade.SetExpertMagicNumber(InpMagicNumber);
    g_trade.SetDeviationInPoints(20);

    g_handleATR = iATR(_Symbol, InpATRTF, InpATRPeriod);
    if(g_handleATR == INVALID_HANDLE) { Print("iATR init thất bại"); return INIT_FAILED; }
    g_handleADX = iADX(_Symbol, InpADXTF, InpADXPeriod);
    if(g_handleADX == INVALID_HANDLE) { Print("iADX init thất bại"); return INIT_FAILED; }

    EventSetTimer(1);

    g_dayStartEquity  = AccountInfoDouble(ACCOUNT_EQUITY);
    g_peakEquityToday = g_dayStartEquity;
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    g_lastDayChecked = StringToTime(StringFormat("%04d.%02d.%02d", now.year, now.mon, now.day));

    RecoverState();
    NotifyStart();
    return INIT_SUCCEEDED;
}

void OnDeinit(const int reason) {
    if(g_handleATR != INVALID_HANDLE) { IndicatorRelease(g_handleATR); g_handleATR = INVALID_HANDLE; }
    if(g_handleADX != INVALID_HANDLE) { IndicatorRelease(g_handleADX); g_handleADX = INVALID_HANDLE; }
    EventKillTimer();
    ObjectsDeleteAll(0, GUI);

    string rsn = "Code " + IntegerToString(reason);
    switch(reason) {
        case REASON_REMOVE:     rsn = "EA bị xóa khỏi chart"; break;
        case REASON_RECOMPILE:  rsn = "Recompile";             break;
        case REASON_CHARTCLOSE: rsn = "Chart đóng";            break;
        case REASON_ACCOUNT:    rsn = "Đổi account";           break;
        case REASON_INITFAILED: rsn = "Init thất bại";         break;
        case REASON_CLOSE:      rsn = "Terminal đóng";         break;
    }
    NotifyStop(rsn);
}

void OnTick() {
    //--- 1. Validate tick ---
    if(!ValidateTick()) return;

    //--- 2. Kết nối broker ---
    bool connected = (bool)TerminalInfoInteger(TERMINAL_CONNECTED);
    if(!connected) {
        if(g_state != STATE_DISCONNECTED) {
            g_prevState = g_state; g_state = STATE_DISCONNECTED; g_disconnectStart = TimeCurrent();
        }
        ManageTrailing();  // SL phía server vẫn active khi mất kết nối
        return;
    }
    if(g_state == STATE_DISCONNECTED) { g_state = g_prevState; RecoverState(); g_disconnectStart = 0; }

    //--- 3. Ngày mới ---
    CheckNewDay();

    //--- 4. Drawdown ---
    UpdateIntradayDD();

    //--- 5. EOD close ---
    if(IsEODClose()) {
        if(g_state != STATE_EOD_CLOSE) {
            g_state = STATE_EOD_CLOSE; CloseAllPositions("EOD"); CancelAllPendingOrders();
        }
        return;
    } else if(g_state == STATE_EOD_CLOSE) g_state = STATE_IDLE;

    //--- 6. Daily limit ---
    if(g_state == STATE_DAILY_STOP) return;

    //--- 7. Pause ---
    CheckPauseExpiry();
    ManageTrailing();
    if(g_state == STATE_PAUSED) return;

    //--- 8. Orphan ---
    CheckOrphanOrder();

    //--- 9. Không overlap ---
    if(CountMyPositions() > 0 || CountMyPendingOrders() > 0) {
        g_state = (CountMyPositions() > 0) ? STATE_POSITION : STATE_WAITING;
        return;
    }

    //--- 10. Buffer EOD ---
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    int curMin = now.hour * 60 + now.min;
    int eodMin = (now.day_of_week == 5)
                ? ((InpFridayEndHour - InTimeGMT) * 60 + InpFridayEndMin)
                : ((InpEndHour       - InTimeGMT) * 60 + InpEndMinute);
    if(curMin >= eodMin - EOD_BUFFER_MIN) return;

    //--- 11. Filters ---
    if(!AllFiltersPass()) { g_state = STATE_IDLE; return; }

    //--- 12. Margin ---
    if(!HasSufficientMargin()) return;

    //--- 13. Đặt straddle ---
    g_state = STATE_PLACING;
    PlaceStraddle(false, 0.0);
}

void OnTimer() {
    CheckPauseExpiry();
    CheckDailySummary();
    if(g_disconnectStart > 0 &&
       TimeCurrent() - g_disconnectStart > DISCONNECT_ALERT_S &&
       CountMyPositions() > 0) {
        NotifyErrorAlert(StringFormat("Mất kết nối > %d giây với position đang mở!", DISCONNECT_ALERT_S));
        g_disconnectStart = TimeCurrent();  // reset để không spam
    }
    UpdateGUI();
}

void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest&     request,
                        const MqlTradeResult&      result) {
    if(trans.type != TRADE_TRANSACTION_DEAL_ADD) return;
    ulong dealTkt = trans.deal;
    if(!HistoryDealSelect(dealTkt)) return;
    if(HistoryDealGetInteger(dealTkt, DEAL_MAGIC)  != InpMagicNumber) return;
    if(HistoryDealGetString(dealTkt,  DEAL_SYMBOL) != _Symbol) return;

    ENUM_DEAL_ENTRY dealEntry = (ENUM_DEAL_ENTRY)HistoryDealGetInteger(dealTkt, DEAL_ENTRY);
    double          pl        = HistoryDealGetDouble(dealTkt,  DEAL_PROFIT);
    ulong           posId     = HistoryDealGetInteger(dealTkt, DEAL_POSITION_ID);
    string          comment   = HistoryDealGetString(dealTkt,  DEAL_COMMENT);
    double          dealPrice = HistoryDealGetDouble(dealTkt,  DEAL_PRICE);

    if(dealEntry == DEAL_ENTRY_IN) {
        g_state = STATE_POSITION; g_lastFillTime = TimeCurrent(); g_filledPosId = posId;
        // Nếu cả 2 leg fill trong < ORPHAN_CHECK_S giây → giữ cả 2 (whipsaw)
        // CheckOrphanOrder() xử lý hủy leg còn lại sau đó
    }

    if(dealEntry == DEAL_ENTRY_OUT || dealEntry == DEAL_ENTRY_OUT_BY) {
        bool isManual    = (StringFind(comment, "close") >= 0 && StringFind(comment, "EOD") < 0);
        bool isEOD       = (StringFind(comment, "EOD")   >= 0);
        bool isDailyStop = (StringFind(comment, "Daily") >= 0 || StringFind(comment, "Force") >= 0);

        if(isManual && !isEOD && !isDailyStop) { UpdateAfterTrade(pl); g_state = STATE_IDLE; return; }

        if(pl > 0 && !isEOD && !isDailyStop) {
            NotifyCloseTP(posId, pl); PeakRemove(posId); UpdateAfterTrade(pl); g_state = STATE_IDLE;
        }
        else if(pl <= 0 && !isEOD && !isDailyStop) {
            NotifyCloseSL(posId, pl);
            double peakForPos = PeakGet(posId);
            PeakRemove(posId);
            // peak < T1Trigger → trailing chưa kích hoạt lần nào → initial SL hit
            bool isInitialSL = (peakForPos < InpT1Trigger);
            if(isInitialSL) {
                UpdateAfterTrade(pl);
                g_state = STATE_RECOVERING;
                if(CountMyPositions() == 0 && CountMyPendingOrders() == 0) TryReset(dealPrice);
            } else {
                // Trailing SL hit → không tính loss streak (đã bảo vệ được phần lãi)
                g_lossStreak = 0; g_totalTrades++; g_grossLoss += pl; g_todayProfit += pl;
                g_state = STATE_IDLE;
            }
        }
        else {
            PeakRemove(posId); UpdateAfterTrade(pl); g_state = STATE_IDLE;
        }
    }
}

void OnChartEvent(const int id, const long& lp, const double& dp, const string& sp) {
    if(id != CHARTEVENT_OBJECT_CLICK) return;
    if(sp == GUI + "BtnCloseAll") { CloseAllPositions("Manual"); CancelAllPendingOrders(); }
    ObjectSetInteger(0, sp, OBJPROP_STATE, false);
    UpdateGUI();
}

//+------------------------------------------------------------------+
//|  GUI                                                             |
//+------------------------------------------------------------------+

void Lbl(string name, string text, int x, int y, color clr = clrSilver, int sz = 9) {
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0) {
        ObjectCreate(0, obj, OBJ_LABEL, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,     CORNER_LEFT_UPPER);
        ObjectSetInteger(0, obj, OBJPROP_BACK,       false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE, false);
    }
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x);
    ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
    ObjectSetString(0,  obj, OBJPROP_TEXT,      text);
    ObjectSetString(0,  obj, OBJPROP_FONT,      "Consolas");
    ObjectSetInteger(0, obj, OBJPROP_FONTSIZE,  sz);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,     clr);
}

void CreateBG(string name, int x, int y, int w, int h, color bg, color border) {
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0) {
        ObjectCreate(0, obj, OBJ_RECTANGLE_LABEL, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
        ObjectSetInteger(0, obj, OBJPROP_BORDER_TYPE, BORDER_FLAT);
        ObjectSetInteger(0, obj, OBJPROP_WIDTH,       1);
        ObjectSetInteger(0, obj, OBJPROP_BACK,        false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE,  false);
    }
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x); ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
    ObjectSetInteger(0, obj, OBJPROP_XSIZE,     w); ObjectSetInteger(0, obj, OBJPROP_YSIZE,     h);
    ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,   bg); ObjectSetInteger(0, obj, OBJPROP_COLOR,    border);
}

void CreateBtn(string name, string text, int x, int y, int w, int h, color bg, color border = clrSilver) {
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0) {
        ObjectCreate(0, obj, OBJ_BUTTON, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,     CORNER_LEFT_UPPER);
        ObjectSetString(0,  obj, OBJPROP_FONT,       "Consolas");
        ObjectSetInteger(0, obj, OBJPROP_FONTSIZE,   9);
        ObjectSetInteger(0, obj, OBJPROP_BACK,       false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE, false);
    }
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x); ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
    ObjectSetInteger(0, obj, OBJPROP_XSIZE,     w); ObjectSetInteger(0, obj, OBJPROP_YSIZE,     h);
    ObjectSetString(0,  obj, OBJPROP_TEXT,       text);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,      clrWhite);
    ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,    bg);
    ObjectSetInteger(0, obj, OBJPROP_BORDER_COLOR, border);
    ObjectSetInteger(0, obj, OBJPROP_STATE,      false);
}

void UpdateGUI() {
    double balance = AccountInfoDouble(ACCOUNT_BALANCE);
    double equity  = AccountInfoDouble(ACCOUNT_EQUITY);
    double spread  = (double)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD);

    double floatPL = 0;
    int    nPos = 0, nPend = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--) {
        if(!g_posInfo.SelectByIndex(i)) continue;
        if(g_posInfo.Magic() != InpMagicNumber || g_posInfo.Symbol() != _Symbol) continue;
        floatPL += g_posInfo.Profit() + g_posInfo.Swap(); nPos++;
    }
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(!g_orderInfo.SelectByIndex(i)) continue;
        if(g_orderInfo.Magic() != InpMagicNumber || g_orderInfo.Symbol() != _Symbol) continue;
        nPend++;
    }

    MqlDateTime dt;
    TimeToStruct(TimeLocal(), dt);
    string tStr = StringFormat("%04d/%02d/%02d  %02d:%02d:%02d", dt.year, dt.mon, dt.day, dt.hour, dt.min, dt.sec);

    string stateStr; color stateClr;
    switch(g_state) {
        case STATE_IDLE:         stateStr = "IDLE — Chờ filter";   stateClr = clrSilver;    break;
        case STATE_PLACING:      stateStr = "PLACING — Đang đặt";  stateClr = clrYellow;    break;
        case STATE_WAITING:      stateStr = "WAITING — Chờ fill";  stateClr = clrYellow;    break;
        case STATE_POSITION:     stateStr = "ACTIVE — Có lệnh";    stateClr = clrLimeGreen; break;
        case STATE_RECOVERING:   stateStr = "RECOVERING";           stateClr = clrOrange;    break;
        case STATE_PAUSED:       stateStr = "PAUSED — Loss streak"; stateClr = clrOrange;    break;
        case STATE_DAILY_STOP:   stateStr = "DAILY STOP";           stateClr = clrTomato;    break;
        case STATE_EOD_CLOSE:    stateStr = "EOD — Đóng lệnh";     stateClr = clrTomato;    break;
        case STATE_DISCONNECTED: stateStr = "DISCONNECTED";         stateClr = clrTomato;    break;
        default:                 stateStr = "UNKNOWN";              stateClr = clrSilver;    break;
    }

    color cFloat  = (floatPL       >= 0) ? clrLimeGreen : clrTomato;
    color cDay    = (g_todayProfit >= 0) ? clrLimeGreen : clrTomato;
    color fcOK    = clrLimeGreen;
    color fcFail  = clrTomato;

    bool bTime = IsInTradingHour();
    bool bRoll = !IsInRolloverWindow();
    bool bSprd = IsSpreadOK();
    bool bATR  = IsATROK();
    bool bADX  = IsADXOK();

    string streakStr;
    if(g_state == STATE_PAUSED && g_pauseUntil > 0) {
        int sec = (int)(g_pauseUntil - TimeCurrent());
        streakStr = (sec > 0) ? StringFormat("%d/%d  (pause %dm%ds)", g_lossStreak, InpMaxLossStreak, sec/60, sec%60)
                              : StringFormat("%d/%d  (resuming...)", g_lossStreak, InpMaxLossStreak);
    } else {
        streakStr = StringFormat("%d / %d", g_lossStreak, InpMaxLossStreak);
    }
    color cStreak = (g_lossStreak >= InpMaxLossStreak)     ? clrTomato
                  : (g_lossStreak >= InpMaxLossStreak - 1) ? clrOrange : clrSilver;

    int pX = InpPanelX, pY = InpPanelY, pW = InpPanelW;
    int s  = InpLineH,  sz = InpFontSz,  cx = pX + 8;

    // ── PANEL 1: TRẠNG THÁI ──
    int p1H = (InpPanel1H > 0) ? InpPanel1H : 10 + (s+2) + (s-3) + 3*s + (s-3) + 3*s + (s-3) + 5*s + 8;
    CreateBG("BG1", pX, pY, pW, p1H, C'14,17,26', C'50,65,120');
    int y = pY + 8;
    Lbl("TT",  "  BOT XAUUSD v2.0",                                  cx, y, C'80,160,255', sz+1); y += s+2;
    Lbl("S0",  "────────────────────────",                            cx, y, C'45,58,105'       ); y += s-3;
    Lbl("Ti",  "Time  : " + tStr,                                     cx, y, clrSilver,     sz  ); y += s;
    Lbl("St",  "State : " + stateStr,                                 cx, y, stateClr,      sz  ); y += s;
    Lbl("Mg",  StringFormat("Magic : %d", InpMagicNumber),            cx, y, C'80,80,80',   sz  ); y += s;
    Lbl("S1",  "────────────────────────",                            cx, y, C'45,58,105'       ); y += s-3;
    Lbl("Bal", StringFormat("Balance : $%.2f", balance),              cx, y, clrSilver,     sz  ); y += s;
    Lbl("Eq",  StringFormat("Equity  : $%.2f", equity),               cx, y, clrSilver,     sz  ); y += s;
    Lbl("DPL", StringFormat("Day P/L : $%.2f", g_todayProfit),        cx, y, cDay,          sz  ); y += s;
    Lbl("S2",  "────────────────────────",                            cx, y, C'45,58,105'       ); y += s-3;
    Lbl("FL",  StringFormat("Float   : $%.2f", floatPL),              cx, y, cFloat,        sz  ); y += s;
    Lbl("Spd", StringFormat("Spread  : %.0f pts", spread),            cx, y, clrSilver,     sz  ); y += s;
    Lbl("Pos", StringFormat("Lệnh    : %d pos  %d pend", nPos, nPend),cx, y, clrSilver,     sz  ); y += s;
    Lbl("Str", "Streak  : " + streakStr,                              cx, y, cStreak,       sz  ); y += s;

    // ── PANEL 2: BỘ LỌC ──
    int p2Y = pY + p1H + InpPanelGap;
    int p2H = (InpPanel2H > 0) ? InpPanel2H : 10 + (s+2) + 5*s + 8;
    CreateBG("BG2", pX, p2Y, pW, p2H, C'17,21,32', C'65,90,160');
    y = p2Y + 8;
    Lbl("FT", "═══  BỘ LỌC  ═══", cx, y, C'90,140,230', sz); y += s+2;
    Lbl("F1", StringFormat("Trading Hour : %s  [%02d-%02d]",         bTime ? "✓ OK " : "✗ OFF",   InpStartHour, InpEndHour),                               cx, y, bTime ? fcOK : fcFail, sz); y += s;
    Lbl("F2", StringFormat("Rollover     : %s  [%02d:%02d-%02d:%02d]",bRoll ? "✓ OK " : "✗ PAUSE",InpRolloverStartH, InpRolloverStartM, InpRolloverEndH, InpRolloverEndM), cx, y, bRoll ? fcOK : fcFail, sz); y += s;
    Lbl("F3", StringFormat("Spread       : %s  [max %d]",            bSprd ? "✓ OK " : "✗ HIGH",  InpMaxSpreadPts),                                        cx, y, bSprd ? fcOK : fcFail, sz); y += s;
    Lbl("F4", StringFormat("ATR          : %s  [%.1f-%.1f]",         bATR  ? "✓ OK " : "✗ BLOCK", InpATRMin, InpATRMax),                                   cx, y, bATR  ? fcOK : fcFail, sz); y += s;
    Lbl("F5", StringFormat("ADX          : %s  [%.0f-%.0f]",         bADX  ? "✓ OK " : "✗ BLOCK", InpADXMin, InpADXMax),                                   cx, y, bADX  ? fcOK : fcFail, sz); y += s;

    // ── PANEL 3: CẤU HÌNH ──
    int p3Y = p2Y + p2H + InpPanelGap;
    int p3H = (InpPanel3H > 0) ? InpPanel3H : 10 + (s+2) + 8*s + 8;
    CreateBG("BG3", pX, p3Y, pW, p3H, C'14,19,28', C'50,70,130');
    y = p3Y + 8;
    Lbl("CT",  "═══  CẤU HÌNH  ═══",                                                    cx, y, C'90,140,230',  sz  ); y += s+2;
    Lbl("C0",  StringFormat("Lot: %.2f  |  SL: %.1f  |  Dist: %.1f", InpLotSize, InpStopLoss, InpStraddleDist), cx, y, clrSilver, sz); y += s;
    Lbl("C1s", "Trailing Tiers:",                                                        cx, y, C'100,125,195', sz  ); y += s;
    Lbl("C1",  StringFormat("T1 ≥%.1f → trail %.1f (dynamic)",       InpT1Trigger, InpT1Trail),                  cx, y, clrSilver, sz); y += s;
    Lbl("C2",  StringFormat("T2 ≥%.1f → lock %.1f",                  InpT2Trigger, InpT2Lock),                   cx, y, clrSilver, sz); y += s;
    Lbl("C3",  StringFormat("T3 ≥%.1f → lock %.1f",                  InpT3Trigger, InpT3Lock),                   cx, y, clrSilver, sz); y += s;
    Lbl("C4",  StringFormat("T4 ≥%.1f → lock %.1f",                  InpT4Trigger, InpT4Lock),                   cx, y, clrSilver, sz); y += s;
    Lbl("C5",  StringFormat("T5 ≥%.1f → lock %.1f",                  InpT5Trigger, InpT5Lock),                   cx, y, clrSilver, sz); y += s;
    Lbl("C6",  StringFormat("T6 ≥%.1f → lock %.0f%% peak (dynamic)", InpT6Trigger, InpT6Lock),                   cx, y, clrYellow, sz); y += s;

    // ── PANEL 4: ĐIỀU KHIỂN ──
    int p4Y = p3Y + p3H + InpPanelGap;
    int p4H = (InpPanel4H > 0) ? InpPanel4H : 10 + (s+2) + 26 + 8;
    CreateBG("BG4", pX, p4Y, pW, p4H, C'17,21,32', C'65,90,160');
    y = p4Y + 8;
    Lbl("BT", "═══  ĐIỀU KHIỂN  ═══", cx, y, C'90,140,230', sz); y += s+2;
    CreateBtn("BtnCloseAll", "  Close All Orders", cx, y, pW - 16, 24, C'20,60,150', C'80,130,230');

    ChartRedraw(0);
}
