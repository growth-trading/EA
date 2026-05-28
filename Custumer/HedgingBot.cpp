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
// Tier 1 — dynamic trail: cắt khi lùi InpT1Trail từ peak
input double InpT1Trigger = 1.0;   // Tier 1 kích hoạt khi peak đạt (đơn vị giá)
input double InpT1Trail   = 0.5;   // Tier 1 cắt khi lùi bao nhiêu từ peak
// Tier 2-6 — fixed lock: khi peak vượt ngưỡng, SL được nâng lên mức lock cố định
input double InpT2Trigger = 1.5;   // Tier 2 kích hoạt khi peak đạt
input double InpT2Lock    = 1.0;   // Tier 2 lock SL tại mức lãi này (tính từ entry)
input double InpT3Trigger = 2.0;   // Tier 3 kích hoạt khi peak đạt
input double InpT3Lock    = 1.0;   // Tier 3 lock SL tại
input double InpT4Trigger = 3.0;   // Tier 4 kích hoạt khi peak đạt
input double InpT4Lock    = 2.0;   // Tier 4 lock SL tại
input double InpT5Trigger = 5.0;   // Tier 5 kích hoạt khi peak đạt
input double InpT5Lock    = 3.5;   // Tier 5 lock SL tại
input double InpT6Trigger = 8.0;   // Tier 6 kích hoạt khi peak đạt
input double InpT6Lock    = 6.0;   // Tier 6 lock SL tại

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
#define WHIPSAW_WINDOW_S     3
#define MARGIN_SAFETY_FACTOR 2.5
#define EOD_BUFFER_MIN       15
#define DAILY_SUMMARY_HOUR   23
#define DAILY_SUMMARY_MIN    0
#define FORCE_CLOSE_PCT      20.0
#define ORPHAN_CHECK_S       3
#define DISCONNECT_ALERT_S   300

//+------------------------------------------------------------------+
//|  STATE MACHINE (9 trạng thái)                                    |
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
//|  GLOBAL STATE VARIABLES                                          |
//+------------------------------------------------------------------+

EA_STATE g_state             = STATE_IDLE;
EA_STATE g_prevState         = STATE_IDLE;

// Risk Management
double   g_dayStartEquity    = 0.0;
double   g_todayProfit       = 0.0;
bool     g_dailyLimitHit     = false;
int      g_lossStreak        = 0;
datetime g_pauseUntil        = 0;
datetime g_lastDayChecked    = 0;

// Entry Throttle
datetime g_lastStraddleTime  = 0;
int      g_straddleInMin     = 0;
datetime g_straddleMinStart  = 0;

// Position Tracking
ulong    g_pendingBuyTkt     = 0;
ulong    g_pendingSellTkt    = 0;
double   g_pendingBuyPx      = 0.0;
double   g_pendingSellPx     = 0.0;
datetime g_lastFillTime      = 0;
ulong    g_filledPosId       = 0;

// Trailing rate limit (global - 1 modify/sec total)
datetime g_lastTrailTime     = 0;

// Reset
datetime g_lastResetTime     = 0;

// Indicators
int      g_handleATR         = INVALID_HANDLE;
int      g_handleADX         = INVALID_HANDLE;

// Statistics
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

// Peak profit tracking per position (để tính tier trailing chính xác)
#define MAX_TRACKED 30
ulong    g_peakTkt[MAX_TRACKED];
double   g_peakPnl[MAX_TRACKED];
int      g_peakCnt = 0;

// MQL5 trade objects
CTrade          g_trade;
CPositionInfo   g_posInfo;
COrderInfo      g_orderInfo;
CSymbolInfo     g_symInfo;

const string GUI = "BOT_";

//+------------------------------------------------------------------+
//|  PRICE / POINT CONVERSION                                        |
//|  QUAN TRỌNG: XAUUSD - 1 giá = 100 điểm (2-digit)                |
//|                       1 giá = 1000 điểm (3-digit)               |
//|  EA tự động detect qua _Point                                    |
//+------------------------------------------------------------------+

/// Chuyển đổi giá -> điểm (tự động theo số digit của symbol)
double PriceToPoints(double price_value) {
    return price_value / _Point;
}

/// Chuyển đổi điểm -> giá (tự động theo số digit của symbol)
double PointsToPrice(double points_value) {
    return points_value * _Point;
}

/// Lấy stop level broker theo đơn vị GIÁ
double GetBrokerStopLevelPrice(){
   long stopPts = SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL);
   return PointsToPrice((double)stopPts);
}

//+------------------------------------------------------------------+
//|  PEAK TRACKING — theo dõi lợi nhuận cao nhất từng position      |
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
//|  CALC LOCK LEVEL — tính mức SL lock dựa trên peak profit        |
//|  Trả về: >= 0 = mức lock (tính từ entry)                        |
//|          -1.0 = chưa có tier nào kích hoạt                      |
//+------------------------------------------------------------------+

double CalcLockLevel(double peak) {
    if(InpT6Trigger > 0 && peak >= InpT6Trigger) return InpT6Lock;
    if(InpT5Trigger > 0 && peak >= InpT5Trigger) return InpT5Lock;
    if(InpT4Trigger > 0 && peak >= InpT4Trigger) return InpT4Lock;
    if(InpT3Trigger > 0 && peak >= InpT3Trigger) return InpT3Lock;
    if(InpT2Trigger > 0 && peak >= InpT2Trigger) return InpT2Lock;
    if(InpT1Trigger > 0 && peak >= InpT1Trigger) return peak - InpT1Trail; // dynamic trail
    return -1.0;
}

//+------------------------------------------------------------------+
//|  TELEGRAM NOTIFICATION SYSTEM                                    |
//+------------------------------------------------------------------+

/// URL encode UTF-8: hỗ trợ ký tự tiếng Việt
string UrlEncode(string text) {
    string result = "";
    uchar  utf8[];
    StringToCharArray(text, utf8, 0, WHOLE_ARRAY, CP_UTF8);
    int uLen = ArraySize(utf8);
    for(int i = 0; i < uLen - 1; i++) {
        // bỏ null terminator cuối
        uchar c = utf8[i];
        if((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
            result += CharToString(c);
        else if(c == ' ')
            result += "+";
        else
            result += StringFormat("%%%02X", (int)c);
        }
    return result;
}

datetime g_lastTelegramTime = 0;

/// Gửi message qua Telegram WebRequest
bool SendTelegram(string text) {
    if(!InpEnableTelegram) return false;
    if(StringLen(InpTelegramToken) < 20) {
        return false;
    }
    // Rate limit: 1 msg/giây
    if(TimeCurrent() - g_lastTelegramTime < 1)
        Sleep(1000);
    string url     = "https://api.telegram.org/bot" + InpTelegramToken + "/sendMessage";
    string headers = "Content-Type: application/x-www-form-urlencoded\r\n";
    string body    = "chat_id=" + InpTelegramChatID
                    + "&text=" + UrlEncode(text)
                    + "&parse_mode=Markdown";
    char   post[], result[];
    StringToCharArray(body, post, 0, StringLen(body));
    string resHeaders;
    int httpCode = WebRequest("POST", url, headers, 5000, post, result, resHeaders);
    g_lastTelegramTime = TimeCurrent();
    if(httpCode == -1) {
        int err = GetLastError();
        return false;
    }
    if(httpCode == 429) {
        Sleep(1000);
        return SendTelegram(text);  // retry 1 lần
    }
    if(httpCode != 200) {
        return false;
    }
    return true;
}

void NotifyStart() {
    SendTelegram(StringFormat(
        "KHỞI ĐỘNG: %s v%s\nSymbol: %s | Lot: %.2f | SL: %.1f | Straddle: %.1f\n"
        "Trailing T1: đạt %.1f -> trail %.1f\n"
        "T2: %.1f->%.1f | T3: %.1f->%.1f\n"
        "T4: %.1f->%.1f | T5: %.1f->%.1f | T6: %.1f->%.1f",
        EA_NAME, EA_VERSION, _Symbol, InpLotSize, InpStopLoss, InpStraddleDist,
        InpT1Trigger, InpT1Trail,
        InpT2Trigger, InpT2Lock, InpT3Trigger, InpT3Lock,
        InpT4Trigger, InpT4Lock, InpT5Trigger, InpT5Lock, InpT6Trigger, InpT6Lock));
}

void NotifyStop(string reason) {
    SendTelegram(StringFormat("DỪNG: %s\nLý do: %s", EA_NAME, reason));
}

void NotifyStraddleSetup(double buyPx, double sellPx) {
    if(!InpNotifyEntry) return;
    SendTelegram(StringFormat(
        "STRADDLE ĐẶT OK\nBuy Stop: %.2f | Sell Stop: %.2f\nSL: %.1f | Trailing 6-tier active",
        buyPx, sellPx, InpStopLoss));
}

void NotifyCloseTP(ulong posId, double profit) {
    if(!InpNotifyExit) return;
    SendTelegram(StringFormat("ĐÓNG LÃI - Pos #%llu\nLãi: +%.2f USD", posId, profit));
}

void NotifyCloseSL(ulong posId, double loss) {
    if(!InpNotifyExit) return;
    SendTelegram(StringFormat("SL HIT - Pos #%llu\nLỗ: %.2f USD", posId, loss));
}

void NotifyResetSetup(double buyPx, double sellPx) {
    if(!InpNotifyReset) return;
    SendTelegram(StringFormat("RESET Straddle\nBuy Stop: %.2f | Sell Stop: %.2f", buyPx, sellPx));
}

void NotifyPause(int streak) {
    SendTelegram(StringFormat(
        "EA TẠM DỪNG\n%d lệnh thua liên tiếp -> pause %d phút", streak, InpPauseMinutes));
}

void NotifyResume() {
    SendTelegram("EA TIẾP TỤC - Hết thời gian pause");
}

void NotifyDailyLimitHit(double pct) {
    SendTelegram(StringFormat(
        "DAILY LIMIT HIT\nĐã lỗ %.1f%% equity ngày -> EA dừng đến nửa đêm", pct));
}

void NotifyDailySummary() {
    if(!InpNotifyDaily) return;
    double wr = (g_totalTrades > 0) ? (100.0 * g_totalWins / g_totalTrades) : 0.0;
    double pf = (g_grossLoss  != 0) ? (g_grossProfit / MathAbs(g_grossLoss)) : 0.0;
    double net = g_grossProfit + g_grossLoss;
    SendTelegram(StringFormat(
        "TỔNG KẾT NGÀY\nTrades: %d | W: %d | L: %d\nWin Rate: %.1f%% | PF: %.2f\nNet PL: %.2f USD\nMax DD: %.2f USD\nFilter rejects: T=%lld R=%lld S=%lld ATR=%lld ADX=%lld",
        g_totalTrades, g_totalWins, g_totalLosses,
        wr, pf, net, g_maxDDIntraday,
        g_rejTime, g_rejRoll, g_rejSpread, g_rejATR, g_rejADX));
}

void NotifyErrorAlert(string msg) {
    SendTelegram("LỖI NGHIÊM TRỌNG\n" + msg);
}

//+------------------------------------------------------------------+
//|  HỆ THỐNG LỌC 5 TẦNG (Short-circuit)                            |
//+------------------------------------------------------------------+

/// Filter 1: Khung thời gian
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

/// Filter 2: Rollover window
bool IsInRolloverWindow() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    int curMin   = now.hour * 60 + now.min;
    int startMin = (InpRolloverStartH - InTimeGMT) * 60 + InpRolloverStartM;
    int endMin   = (InpRolloverEndH - InTimeGMT)  * 60 + InpRolloverEndM;
    return (curMin >= startMin && curMin <= endMin);
}

/// Filter 3: Spread
bool IsSpreadOK() {
    int spread = (int)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD);
    return (spread <= InpMaxSpreadPts);
}

/// Filter 4: ATR biến động
double GetATR() {
    double buf[];
    if(CopyBuffer(g_handleATR, 0, 0, 1, buf) != 1) return 0.0;
    return buf[0];
}

bool IsATROK() {
    double atr = GetATR();
    if(atr <= 0.0) return false;
    return (atr >= InpATRMin && atr <= InpATRMax);
}

/// Filter 5: ADX đo mạnh xu hướng
double GetADX() {
    double buf[];
    // Buffer 0 = ADX main line (MODE_MAIN)
    if(CopyBuffer(g_handleADX, 0, 0, 1, buf) != 1) return 0.0;
    return buf[0];
}

bool IsADXOK() {
    double adx = GetADX();
    if(adx <= 0.0) return false;
    // Exclusive bounds: NGHIÊM NGẶT > min VÀ < max
    return (adx > InpADXMin && adx < InpADXMax);
}

/// Đánh giá tất cả 5 filter theo thứ tự, short-circuit khi fail
bool AllFiltersPass() {
    if(!IsInTradingHour())     { g_rejTime++;   return false; }
    if(IsInRolloverWindow())   { g_rejRoll++;   return false; }
    if(!IsSpreadOK())          { g_rejSpread++; return false; }
    if(!IsATROK())             { g_rejATR++;    return false; }
    if(!IsADXOK())             { g_rejADX++;    return false; }
    return true;
}

//+------------------------------------------------------------------+
//|  TRAILING STOP PHÂN TẦNG (6 tầng)                               |
//+------------------------------------------------------------------+

/// Áp dụng trailing 6-tier cho 1 position
void ApplyTrailing(ulong ticket) {
    if(!InpEnableTrailing) return;
    if(!g_posInfo.SelectByTicket(ticket)) return;
    if(g_posInfo.Magic() != InpMagicNumber) return;
    if(g_posInfo.Symbol() != _Symbol) return;

    double entry = g_posInfo.PriceOpen();
    double curSL = g_posInfo.StopLoss();
    ENUM_POSITION_TYPE pType = g_posInfo.PositionType();

    double mktPx = (pType == POSITION_TYPE_BUY)
                    ? SymbolInfoDouble(_Symbol, SYMBOL_BID)
                    : SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double profit = (pType == POSITION_TYPE_BUY)
                    ? (mktPx - entry)
                    : (entry - mktPx);

    // Cập nhật peak profit cho ticket này
    double peak = PeakGet(ticket);
    if(profit > peak) { peak = profit; PeakSet(ticket, peak); }

    double lockLevel = CalcLockLevel(peak);
    if(lockLevel < 0.0) return; // chưa có tier nào kích hoạt

    double newSL = (pType == POSITION_TYPE_BUY)
                    ? NormalizeDouble(entry + lockLevel, _Digits)
                    : NormalizeDouble(entry - lockLevel, _Digits);

    // Chỉ modify khi SL mới tốt hơn SL hiện tại (chỉ đi 1 chiều)
    bool shouldModify = false;
    if(pType == POSITION_TYPE_BUY  && newSL > curSL + _Point) shouldModify = true;
    if(pType == POSITION_TYPE_SELL && newSL < curSL - _Point) shouldModify = true;
    if(!shouldModify) return;

    // Rate limit: tối đa 1 modify/giây
    if(TimeCurrent() - g_lastTrailTime < TRAIL_RATE_LIMIT_S) return;
    if(g_trade.PositionModify(ticket, newSL, 0))
        g_lastTrailTime = TimeCurrent();
}

/// Áp dụng trailing cho tất cả position đang mở
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
        if(g_posInfo.SelectByIndex(i) &&
            g_posInfo.Magic() == InpMagicNumber &&
            g_posInfo.Symbol() == _Symbol)
        cnt++;
    return cnt;
}

int CountMyPendingOrders() {
    int cnt = 0;
    for(int i = OrdersTotal() - 1; i >= 0; i--)
        if(g_orderInfo.SelectByIndex(i) &&
            g_orderInfo.Magic() == InpMagicNumber &&
            g_orderInfo.Symbol() == _Symbol)
        cnt++;
    return cnt;
}

//+------------------------------------------------------------------+
//|  ORDER OPERATIONS                                                |
//+------------------------------------------------------------------+

void CloseAllPositions(string reason) {
    for(int i = PositionsTotal() - 1; i >= 0; i--) {
        if(g_posInfo.SelectByIndex(i) &&
            g_posInfo.Magic() == InpMagicNumber &&
            g_posInfo.Symbol() == _Symbol) {
            if(!g_trade.PositionClose(g_posInfo.Ticket())) Print("");
        }
    }
}

void CancelAllPendingOrders() {
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(g_orderInfo.SelectByIndex(i) &&
            g_orderInfo.Magic() == InpMagicNumber &&
            g_orderInfo.Symbol() == _Symbol) {
            ulong tk = g_orderInfo.Ticket();
            if(!g_trade.OrderDelete(tk)) Print("");
        }
    }
}

/// Hủy 1 pending order, có retry (chống orphan)
bool CancelOrderSafe(ulong ticket) {
    for(int attempt = 0; attempt < 2; attempt++) {
        if(g_trade.OrderDelete(ticket)) return true;
        Sleep(RETRY_DELAY_MS);
    }
    NotifyErrorAlert(StringFormat("ORPHAN — Không hủy được order #%llu!", ticket));
    return false;
}

//+------------------------------------------------------------------+
//|  MARGIN CHECK                                                    |
//+------------------------------------------------------------------+

bool HasSufficientMargin() {
    double mgnReq = 0.0;
    double px = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    if(!OrderCalcMargin(ORDER_TYPE_BUY, _Symbol, InpLotSize, px, mgnReq)) {
        return false;
    }
    double freeMgn = AccountInfoDouble(ACCOUNT_MARGIN_FREE);
    if(freeMgn < mgnReq * MARGIN_SAFETY_FACTOR) {
        return false;
    }
    return true;
}

//+------------------------------------------------------------------+
//|  BROKER ERROR HANDLING                                           |
//+------------------------------------------------------------------+

bool HandleRetcode(uint retcode, string ctx) {
    switch(retcode) {
        case TRADE_RETCODE_DONE:
        case TRADE_RETCODE_PLACED:
            return true;
        case TRADE_RETCODE_REQUOTE:
        case TRADE_RETCODE_PRICE_CHANGED:
            Sleep(RETRY_DELAY_MS);
            return false;
        case TRADE_RETCODE_NO_MONEY:
            NotifyErrorAlert(ctx + ": Tài khoản không đủ margin!");
            return false;
        case TRADE_RETCODE_INVALID_STOPS:
            return false;
        case TRADE_RETCODE_MARKET_CLOSED:
            return false;
        default:
            return false;
        }
}

//+------------------------------------------------------------------+
//|  LOGIC VÀO LỆNH (ĐẶT STRADDLE) — ATOMIC                        |
//+------------------------------------------------------------------+

/// Đặt straddle nguyên tử (Buy Stop + Sell Stop)
/// isReset: true = straddle reset sau SL
bool PlaceStraddle(bool isReset, double resetAtPrice) {
    //--- Throttle chống spam ---
    if(!isReset) {
        if(TimeCurrent() - g_lastStraddleTime < STRADDLE_THROTTLE_S)
            return false;

        if(TimeCurrent() - g_straddleMinStart < 60){
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

    //--- Refresh rates và validate tick ---
    if(!g_symInfo.RefreshRates()) return false;
    double bid = g_symInfo.Bid();
    double ask = g_symInfo.Ask();
    if(bid <= 0 || ask <= 0 || ask <= bid) return false;

    //--- Tính giá đặt lệnh ---
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

    // Adjust nếu vi phạm stop level
    if(buyPx - ask < minDist)
        buyPx = NormalizeDouble(ask + minDist, _Digits);
    if(bid - sellPx < minDist)
        sellPx = NormalizeDouble(bid - minDist, _Digits);

    buyPx  = NormalizeDouble(buyPx,  _Digits);
    sellPx = NormalizeDouble(sellPx, _Digits);

    //--- SL server-side, TP = 0 (quản lý bởi trailing tier) ---
    double sl_buy  = NormalizeDouble(buyPx  - InpStopLoss, _Digits);
    double sl_sell = NormalizeDouble(sellPx + InpStopLoss, _Digits);

    string sfx   = isReset ? "_R" : "";
    string cBuy  = InpComment + "_BUY"  + sfx;
    string cSell = InpComment + "_SELL" + sfx;

    g_trade.SetExpertMagicNumber(InpMagicNumber);
    g_trade.SetDeviationInPoints(20);
    g_trade.SetTypeFilling(ORDER_FILLING_RETURN);

    //--- Đặt BUY STOP ---
    bool  buyOK  = false;
    ulong buyTkt = 0;
    for(int att = 0; att < MAX_RETRY_COUNT; att++) {
        buyOK = g_trade.BuyStop(InpLotSize, buyPx, _Symbol, sl_buy, 0,
                              ORDER_TIME_GTC, 0, cBuy);
        if(buyOK) { buyTkt = g_trade.ResultOrder(); break; }
        if(!HandleRetcode(g_trade.ResultRetcode(), "BuyStop")) break;
    }
    if(!buyOK || buyTkt == 0) return false;

    //--- Đặt SELL STOP ---
    bool  sellOK  = false;
    ulong sellTkt = 0;
    for(int att = 0; att < MAX_RETRY_COUNT; att++) {
        sellOK = g_trade.SellStop(InpLotSize, sellPx, _Symbol, sl_sell, 0,
                                    ORDER_TIME_GTC, 0, cSell);
        if(sellOK) { sellTkt = g_trade.ResultOrder(); break; }
        if(!HandleRetcode(g_trade.ResultRetcode(), "SellStop")) break;
    }

    //--- ATOMICITY: nếu SellStop thất bại, ROLLBACK BuyStop ---
    if(!sellOK || sellTkt == 0) {
        if(!CancelOrderSafe(buyTkt))
            NotifyErrorAlert(StringFormat("ORPHAN BuyStop #%llu — cần kiểm tra thủ công!", buyTkt));
        return false;
    }

    //--- Lưu tracking ---
    g_pendingBuyTkt  = buyTkt;
    g_pendingSellTkt = sellTkt;
    g_pendingBuyPx   = buyPx;
    g_pendingSellPx  = sellPx;

    if(isReset)
        NotifyResetSetup(buyPx, sellPx);
    else
        NotifyStraddleSetup(buyPx, sellPx);

    g_state = STATE_WAITING;
    return true;
}

//+------------------------------------------------------------------+
//|  LOGIC RESET                                                     |
//+------------------------------------------------------------------+

void TryReset(double slHitPrice) {
    if(g_lossStreak >= InpMaxLossStreak)  return;
    if(g_dailyLimitHit)  return;
    if(!IsInTradingHour()) return;
    if(!AllFiltersPass()) return;
    if(TimeCurrent() - g_lastResetTime <= 1) return;
    if(CountMyPositions() > 0 || CountMyPendingOrders() > 0) return;

    g_lastResetTime = TimeCurrent();
    PlaceStraddle(true, slHitPrice);
}

//+------------------------------------------------------------------+
//|  QUẢN LÝ RỦI RO                                                 |
//+------------------------------------------------------------------+

void CheckNewDay() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    datetime today = StringToTime(StringFormat("%04d.%02d.%02d", now.year, now.mon, now.day));
    if(today == g_lastDayChecked) return;
    g_lastDayChecked   = today;
    g_dayStartEquity   = AccountInfoDouble(ACCOUNT_EQUITY);
    g_todayProfit      = 0.0;
    g_dailyLimitHit    = false;
    g_totalTrades      = 0;
    g_totalWins        = 0;
    g_totalLosses      = 0;
    g_grossProfit      = 0.0;
    g_grossLoss        = 0.0;
    g_maxDDIntraday    = 0.0;
    g_peakEquityToday  = g_dayStartEquity;
    g_dailySummarySent = false;
    g_rejTime          = 0;
    g_rejRoll          = 0;
    g_rejSpread        = 0;
    g_rejATR           = 0;
    g_rejADX           = 0;
}

/// Cập nhật max drawdown intraday
void UpdateIntradayDD() {
    double eq = AccountInfoDouble(ACCOUNT_EQUITY);
    if(eq > g_peakEquityToday) g_peakEquityToday = eq;
    double dd = g_peakEquityToday - eq;
    if(dd > g_maxDDIntraday) g_maxDDIntraday = dd;
}

/// Kiểm tra daily loss limit
void CheckDailyLossLimit() {
    if(g_dailyLimitHit || g_dayStartEquity <= 0) return;

    double eq        = AccountInfoDouble(ACCOUNT_EQUITY);
    double totalDD   = g_dayStartEquity - eq;
    double totalDDPct = (totalDD / g_dayStartEquity) * 100.0;
    // Force-close nếu total drawdown (đã chốt + floating) > 20%
    if(totalDDPct >= FORCE_CLOSE_PCT) {
        CloseAllPositions("ForceClose DD>20%");
        CancelAllPendingOrders();
    }

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

/// Cập nhật loss streak và stats sau khi trade đóng
void UpdateAfterTrade(double closedPL) {
    g_totalTrades++;
    if(closedPL > 0) {
        g_totalWins++;
        g_grossProfit += closedPL;
        g_todayProfit += closedPL;
        g_lossStreak  = 0;
    } else {
        g_totalLosses++;
        g_grossLoss   += closedPL;
        g_todayProfit += closedPL;
        g_lossStreak++;

        if(g_lossStreak >= InpMaxLossStreak) {
            g_pauseUntil = TimeCurrent() + (datetime)(InpPauseMinutes * 60);
            g_state      = STATE_PAUSED;
            CancelAllPendingOrders();
            NotifyPause(g_lossStreak);
        }
    }
    CheckDailyLossLimit();
}

/// Kiểm tra và xử lý khi pause hết hạn
void CheckPauseExpiry() {
    if(g_state != STATE_PAUSED) return;
    if(g_pauseUntil > 0 && TimeCurrent() >= g_pauseUntil) {
        g_pauseUntil = 0;
        g_state      = STATE_IDLE;
        NotifyResume();
    }
}

/// Kiểm tra EOD close
bool IsEODClose() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    if(now.day_of_week == 0 || now.day_of_week == 6) return true;
    int cur = now.hour * 60 + now.min;
    int eod = (now.day_of_week == 5)
                ? ((InpFridayEndHour - InTimeGMT) * 60 + InpFridayEndMin)
                : ((InpEndHour - InTimeGMT) * 60 + InpEndMinute);
    return (cur >= eod);
}

//+------------------------------------------------------------------+
//|  SPECIAL CASES & ERROR HANDLING                                  |
//+------------------------------------------------------------------+

/// Kiểm tra orphan order: sau ORPHAN_CHECK_S giây, hủy leg còn lại
void CheckOrphanOrder() {
    if(g_lastFillTime == 0) return;
    if(TimeCurrent() - g_lastFillTime < (datetime)ORPHAN_CHECK_S) return;
    // Tìm pending order còn lại của EA mà không phải pos đã fill
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(g_orderInfo.SelectByIndex(i) &&
            g_orderInfo.Magic() == InpMagicNumber &&
            g_orderInfo.Symbol() == _Symbol) {
            ulong tk = g_orderInfo.Ticket();
            CancelOrderSafe(tk);
        }
    }
    g_lastFillTime = 0;
    g_filledPosId  = 0;
}

/// Validate tick
bool ValidateTick() {
    if(!g_symInfo.RefreshRates()) return false;
    if(g_symInfo.Bid() == 0.0 || g_symInfo.Ask() == 0.0) return false;
    if(g_symInfo.Ask() <= g_symInfo.Bid()) {
        return false;
    }
    return true;
}

//+------------------------------------------------------------------+
//|  INPUT VALIDATION                                                |
//+------------------------------------------------------------------+

bool ValidateInputs() {
    bool ok = true;
    double volMin = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
    double volMax = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
    if(InpLotSize < volMin || InpLotSize > volMax) {
        Print("INPUT ERROR: InpLotSize=", InpLotSize, " phải trong [", volMin, ", ", volMax, "]");
        ok = false;
    }
    if(InpStopLoss <= 0)
        { Print("INPUT ERROR: InpStopLoss phải > 0"); ok = false; }
    if(InpStraddleDist <= 0)
        { Print("INPUT ERROR: InpStraddleDist phải > 0"); ok = false; }
    // Validate trailing tiers
    if(InpT1Trigger <= 0 || InpT1Trail <= 0 || InpT1Trail >= InpT1Trigger)
        { Print("INPUT ERROR: T1 — Trigger phải > 0, Trail phải > 0 và < Trigger"); ok = false; }
    if(InpT2Trigger > 0 && InpT2Trigger <= InpT1Trigger)
        { Print("INPUT ERROR: T2Trigger phải > T1Trigger"); ok = false; }
    if(InpT3Trigger > 0 && InpT3Trigger <= InpT2Trigger)
        { Print("INPUT ERROR: T3Trigger phải > T2Trigger"); ok = false; }
    if(InpT4Trigger > 0 && InpT4Trigger <= InpT3Trigger)
        { Print("INPUT ERROR: T4Trigger phải > T3Trigger"); ok = false; }
    if(InpT5Trigger > 0 && InpT5Trigger <= InpT4Trigger)
        { Print("INPUT ERROR: T5Trigger phải > T4Trigger"); ok = false; }
    if(InpT6Trigger > 0 && InpT6Trigger <= InpT5Trigger)
        { Print("INPUT ERROR: T6Trigger phải > T5Trigger"); ok = false; }
    if(InpATRMin >= InpATRMax)
        { Print("INPUT ERROR: InpATRMin phải < InpATRMax"); ok = false; }
    if(InpADXMin >= InpADXMax || InpADXMin < 0 || InpADXMax > 100)
        { Print("INPUT ERROR: ADX range không hợp lệ"); ok = false; }
    if(InpDailyLossPct <= 0 || InpDailyLossPct >= 100)
        { Print("INPUT ERROR: InpDailyLossPct phải trong (0, 100)"); ok = false; }
    if(InpStartHour >= InpEndHour || InpStartHour < 0 || InpEndHour > 23)
        { Print("INPUT ERROR: StartHour/EndHour không hợp lệ"); ok = false; }
    if(InpEnableTelegram && StringLen(InpTelegramToken) < 40)
        { Print("INPUT WARN: Telegram bật nhưng Token có vẻ không đủ dài (< 40 ký tự)"); }
    return ok;
}

//+------------------------------------------------------------------+
//|  ACCOUNT TYPE VALIDATION                                         |
//+------------------------------------------------------------------+

void ValidateAccountType() {
    long mode = AccountInfoInteger(ACCOUNT_TRADE_MODE);
    if(mode == ACCOUNT_TRADE_MODE_CONTEST)
        Print("CONTEST account — một số tính năng có thể bị giới hạn");
    else if(mode == ACCOUNT_TRADE_MODE_DEMO)
        Print("DEMO account — EA đang chạy trên tài khoản demo");
    else
        Print("REAL account — EA đang chạy trên tài khoản thật");
}

//+------------------------------------------------------------------+
//|  STATE RECOVERY SAU RESTART                                      |
//+------------------------------------------------------------------+

void RecoverState() {
    int posCnt = CountMyPositions();
    int ordCnt = CountMyPendingOrders();
    if(posCnt > 0)      g_state = STATE_POSITION;
    else if(ordCnt > 0) g_state = STATE_WAITING;
    else                g_state = STATE_IDLE;

    // Recover loss streak từ lịch sử hôm nay
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
            ENUM_DEAL_ENTRY de = (ENUM_DEAL_ENTRY)HistoryDealGetInteger(dt, DEAL_ENTRY);
            if(de != DEAL_ENTRY_OUT) continue;
            double pl = HistoryDealGetDouble(dt, DEAL_PROFIT);
            if(pl < 0) streak++;
            else { streak = 0; break; }  // thắng cuối -> reset streak
        }
        g_lossStreak = streak;
    } else {
        g_lossStreak = 0;
    }
}

//+------------------------------------------------------------------+
//|  DAILY SUMMARY & HOUSEKEEPING                                    |
//+------------------------------------------------------------------+

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
//|  MAIN EVENT HANDLERS                                             |
//+------------------------------------------------------------------+

int OnInit() {
    if(!ValidateInputs())
        return INIT_PARAMETERS_INCORRECT;

    ValidateAccountType();

    if(!g_symInfo.Name(_Symbol)) {
        Print("Lỗi khởi tạo CSymbolInfo cho: ", _Symbol);
        return INIT_FAILED;
    }

    g_trade.SetExpertMagicNumber(InpMagicNumber);
    g_trade.SetDeviationInPoints(20);

    //--- Khởi tạo indicator handles (một lần duy nhất) ---
    g_handleATR = iATR(_Symbol, InpATRTF, InpATRPeriod);
    if(g_handleATR == INVALID_HANDLE) {
        Print("iATR init thất bại — kiểm tra lại ATR period/timeframe");
        return INIT_FAILED;
    }
    g_handleADX = iADX(_Symbol, InpADXTF, InpADXPeriod);
    if(g_handleADX == INVALID_HANDLE) {
        Print("iADX init thất bại — kiểm tra lại ADX period/timeframe");
        return INIT_FAILED;
    }

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

//+------------------------------------------------------------------+
//| OnDeinit — Tắt EA                                               |
//+------------------------------------------------------------------+
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

//+------------------------------------------------------------------+
//| OnTick — Vòng lặp chính                                         |
//+------------------------------------------------------------------+
void OnTick(){
    //--- 1. Validate tick ---
    if(!ValidateTick()) return;

    //--- 2. Kiểm tra kết nối broker ---
    bool connected = (bool)TerminalInfoInteger(TERMINAL_CONNECTED);
    if(!connected) {
        if(g_state != STATE_DISCONNECTED) {
            g_prevState       = g_state;
            g_state           = STATE_DISCONNECTED;
            g_disconnectStart = TimeCurrent();
        }
        ManageTrailing();  // SL phía server vẫn active, trailing vẫn chạy
        return;
    }
    if(g_state == STATE_DISCONNECTED) {
        g_state = g_prevState;
        RecoverState();
        g_disconnectStart = 0;
    }

    //--- 3. Kiểm tra ngày mới ---
    CheckNewDay();

    //--- 4. Cập nhật drawdown ---
    UpdateIntradayDD();

    //--- 5. EOD close? ---
    if(IsEODClose()) {
        if(g_state != STATE_EOD_CLOSE) {
            g_state = STATE_EOD_CLOSE;
            CloseAllPositions("EOD");
            CancelAllPendingOrders();
        }
        return;
    } else if(g_state == STATE_EOD_CLOSE)
        g_state = STATE_IDLE;

    //--- 6. Daily limit hit? ---
    if(g_state == STATE_DAILY_STOP) return;

    //--- 7. Pause check ---
    CheckPauseExpiry();

    //--- Trailing luôn chạy kể cả khi pause (nếu có position) ---
    ManageTrailing();

    if(g_state == STATE_PAUSED) return;

    //--- 8. Kiểm tra orphan order ---
    CheckOrphanOrder();

    //--- 9. Có position hoặc pending? -> không overlap ---
    if(CountMyPositions() > 0 || CountMyPendingOrders() > 0) {
        g_state = (CountMyPositions() > 0) ? STATE_POSITION : STATE_WAITING;
        return;
    }

    //--- 10. Buffer 15 phút trước EOD -> không entry mới ---
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    int curMin = now.hour * 60 + now.min;
    int eodMin = (now.day_of_week == 5)
                ? ((InpFridayEndHour - InTimeGMT) * 60 + InpFridayEndMin)
                : ((InpEndHour - InTimeGMT) * 60 + InpEndMinute);
    if(curMin >= eodMin - EOD_BUFFER_MIN) return;

    //--- 11. Check tất cả 5 filter ---
    if(!AllFiltersPass()) { g_state = STATE_IDLE; return; }

    //--- 12. Margin check ---
    if(!HasSufficientMargin()) return;

    //--- 13. Đặt straddle ---
    g_state = STATE_PLACING;
    PlaceStraddle(false, 0.0);
}

//+------------------------------------------------------------------+
//| OnTimer — Housekeeping mỗi giây                                 |
//+------------------------------------------------------------------+
void OnTimer() {
    CheckPauseExpiry();
    CheckDailySummary();

    //--- Alert nếu mất kết nối > 5 phút có position ---
    if(g_disconnectStart > 0 &&
        TimeCurrent() - g_disconnectStart > DISCONNECT_ALERT_S &&
        CountMyPositions() > 0) {
        NotifyErrorAlert(StringFormat("Mất kết nối > %d giây với position đang mở!", DISCONNECT_ALERT_S));
        g_disconnectStart = TimeCurrent();  // reset timer để không spam
    }

    UpdateGUI();
}

//+------------------------------------------------------------------+
//| OnTradeTransaction — Phát hiện fill, close, cập nhật stats      |
//+------------------------------------------------------------------+
void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest&     request,
                        const MqlTradeResult&      result){
    //--- Chỉ xử lý DEAL_ADD (giao dịch đã thực thi) ---
    if(trans.type != TRADE_TRANSACTION_DEAL_ADD) return;
    ulong dealTkt = trans.deal;
    if(!HistoryDealSelect(dealTkt)) return;
    if(HistoryDealGetInteger(dealTkt, DEAL_MAGIC)  != InpMagicNumber) return;
    if(HistoryDealGetString(dealTkt,  DEAL_SYMBOL) != _Symbol) return;

    ENUM_DEAL_ENTRY dealEntry = (ENUM_DEAL_ENTRY)HistoryDealGetInteger(dealTkt, DEAL_ENTRY);
    ENUM_DEAL_TYPE  dealType  = (ENUM_DEAL_TYPE) HistoryDealGetInteger(dealTkt, DEAL_TYPE);
    double          pl        = HistoryDealGetDouble(dealTkt, DEAL_PROFIT);
    ulong           posId     = HistoryDealGetInteger(dealTkt, DEAL_POSITION_ID);
    string          comment   = HistoryDealGetString(dealTkt,  DEAL_COMMENT);
    double          dealPrice = HistoryDealGetDouble(dealTkt,  DEAL_PRICE);

    //=== DEAL IN: pending order kích hoạt thành position ===
    if(dealEntry == DEAL_ENTRY_IN) {
        g_state        = STATE_POSITION;
        g_lastFillTime = TimeCurrent();
        g_filledPosId  = posId;
        // Whipsaw check: nếu cả 2 leg fill trong < 3 giây -> giữ cả 2
        // CheckOrphanOrder() sẽ xử lý hủy leg còn lại sau ORPHAN_CHECK_S giây
    }

    //=== DEAL OUT: position đóng ===
    if(dealEntry == DEAL_ENTRY_OUT || dealEntry == DEAL_ENTRY_OUT_BY) {
        bool isManual    = (StringFind(comment, "close") >= 0 && StringFind(comment, "EOD") < 0);
        bool isEOD       = (StringFind(comment, "EOD") >= 0);
        bool isDailyStop = (StringFind(comment, "Daily") >= 0 || StringFind(comment, "Force") >= 0);

        //--- Manual close: log và skip reset ---
        if(isManual && !isEOD && !isDailyStop) {
            UpdateAfterTrade(pl);
            g_state = STATE_IDLE;
            return;
        }

        //--- Trailing SL hit với lãi (peak qua T1, đóng dương) ---
        if(pl > 0 && !isEOD && !isDailyStop) {
            NotifyCloseTP(posId, pl);
            PeakRemove(posId);
            UpdateAfterTrade(pl);
            g_state = STATE_IDLE;
        }
        //--- SL hit (initial SL hoặc trailing SL) ---
        else if(pl <= 0 && !isEOD && !isDailyStop) {
            NotifyCloseSL(posId, pl);

            // Nếu peak chưa vượt T1Trigger -> initial SL hit (trailing chưa kích hoạt lần nào)
            // Nếu peak đã vượt T1Trigger   -> trailing SL hit (đã bảo vệ được phần lãi)
            double peakForPos = PeakGet(posId);
            PeakRemove(posId);
            bool isInitialSL = (peakForPos < InpT1Trigger);

            if(isInitialSL) {
                UpdateAfterTrade(pl);
                g_state = STATE_RECOVERING;
                if(CountMyPositions() == 0 && CountMyPendingOrders() == 0)
                    TryReset(dealPrice);
            } else {
                // Trailing SL hit -> không tính vào loss streak
                g_lossStreak = 0;
                g_totalTrades++;
                g_grossLoss   += pl;
                g_todayProfit += pl;
                g_state = STATE_IDLE;
            }
        }
        //--- EOD / Daily Stop close ---
        else {
            PeakRemove(posId);
            UpdateAfterTrade(pl);
            g_state = STATE_IDLE;
        }
    }
}

//+------------------------------------------------------------------+
//| OnChartEvent — Xử lý click nút                                  |
//+------------------------------------------------------------------+
void OnChartEvent(const int id, const long& lp, const double& dp, const string& sp) {
    if(id != CHARTEVENT_OBJECT_CLICK) return;
    if(sp == GUI + "BtnCloseAll") {
        CloseAllPositions("Manual");
        CancelAllPendingOrders();
    }
    ObjectSetInteger(0, sp, OBJPROP_STATE, false);
    UpdateGUI();
}

//+------------------------------------------------------------------+
//|  GUI — Panel hệ thống                                           |
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
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x);
    ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
    ObjectSetInteger(0, obj, OBJPROP_XSIZE,     w);
    ObjectSetInteger(0, obj, OBJPROP_YSIZE,     h);
    ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,   bg);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,     border);
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
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE,  x);
    ObjectSetInteger(0, obj, OBJPROP_YDISTANCE,  y);
    ObjectSetInteger(0, obj, OBJPROP_XSIZE,      w);
    ObjectSetInteger(0, obj, OBJPROP_YSIZE,      h);
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

    // Tính floating từ positions đang mở
    double floatPL = 0;
    int    nPos = 0, nPend = 0;
    for(int i = PositionsTotal() - 1; i >= 0; i--) {
        if(!g_posInfo.SelectByIndex(i)) continue;
        if(g_posInfo.Magic() != InpMagicNumber || g_posInfo.Symbol() != _Symbol) continue;
        floatPL += g_posInfo.Profit() + g_posInfo.Swap();
        nPos++;
    }
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(!g_orderInfo.SelectByIndex(i)) continue;
        if(g_orderInfo.Magic() != InpMagicNumber || g_orderInfo.Symbol() != _Symbol) continue;
        nPend++;
    }

    MqlDateTime dt;
    TimeToStruct(TimeLocal(), dt);
    string tStr = StringFormat("%04d/%02d/%02d  %02d:%02d:%02d",
                               dt.year, dt.mon, dt.day, dt.hour, dt.min, dt.sec);

    // Trạng thái EA
    string stateStr; color stateClr;
    switch(g_state) {
        case STATE_IDLE:         stateStr = "IDLE — Chờ filter";      stateClr = clrSilver;     break;
        case STATE_PLACING:      stateStr = "PLACING — Đang đặt";     stateClr = clrYellow;     break;
        case STATE_WAITING:      stateStr = "WAITING — Chờ fill";     stateClr = clrYellow;     break;
        case STATE_POSITION:     stateStr = "ACTIVE — Có lệnh";       stateClr = clrLimeGreen;  break;
        case STATE_RECOVERING:   stateStr = "RECOVERING";              stateClr = clrOrange;     break;
        case STATE_PAUSED:       stateStr = "PAUSED — Loss streak";    stateClr = clrOrange;     break;
        case STATE_DAILY_STOP:   stateStr = "DAILY STOP";              stateClr = clrTomato;     break;
        case STATE_EOD_CLOSE:    stateStr = "EOD — Đóng lệnh";        stateClr = clrTomato;     break;
        case STATE_DISCONNECTED: stateStr = "DISCONNECTED";            stateClr = clrTomato;     break;
        default:                 stateStr = "UNKNOWN";                 stateClr = clrSilver;     break;
    }

    // Màu P/L
    color cFloat = (floatPL   >= 0) ? clrLimeGreen : clrTomato;
    color cDay   = (g_todayProfit >= 0) ? clrLimeGreen : clrTomato;

    // Trạng thái bộ lọc
    bool   bTime  = IsInTradingHour();
    bool   bRoll  = !IsInRolloverWindow();
    bool   bSprd  = IsSpreadOK();
    bool   bATR   = IsATROK();
    bool   bADX   = IsADXOK();
    bool   allOK  = (bTime && bRoll && bSprd && bATR && bADX);
    color  fcOK   = clrLimeGreen;
    color  fcFail = clrTomato;

    // Nếu có pause, hiển thị thời gian còn lại
    string streakStr;
    if(g_state == STATE_PAUSED && g_pauseUntil > 0) {
        int sec = (int)(g_pauseUntil - TimeCurrent());
        if(sec > 0)
            streakStr = StringFormat("%d/%d  (pause %dm%ds)",
                        g_lossStreak, InpMaxLossStreak, sec/60, sec%60);
        else
            streakStr = StringFormat("%d/%d  (resuming...)", g_lossStreak, InpMaxLossStreak);
    } else {
        streakStr = StringFormat("%d / %d", g_lossStreak, InpMaxLossStreak);
    }
    color cStreak = (g_lossStreak >= InpMaxLossStreak) ? clrTomato
                  : (g_lossStreak >= InpMaxLossStreak - 1) ? clrOrange
                  : clrSilver;

    int pX  = InpPanelX;
    int pY  = InpPanelY;
    int pW  = InpPanelW;
    int s   = InpLineH;
    int sz  = InpFontSz;
    int cx  = pX + 8;

    // ── PANEL 1: TRẠNG THÁI ──
    int p1H = (InpPanel1H > 0) ? InpPanel1H
            : 10 + (s+2) + (s-3) + 3*s + (s-3) + 3*s + (s-3) + 5*s + 8;
    CreateBG("BG1", pX, pY, pW, p1H, C'14,17,26', C'50,65,120');

    int y = pY + 8;
    Lbl("TT",  "  BOT XAUUSD v2.0",                  cx, y, C'80,160,255', sz+1); y += s+2;
    Lbl("S0",  "────────────────────────",            cx, y, C'45,58,105'        ); y += s-3;
    Lbl("Ti",  "Time  : " + tStr,                    cx, y, clrSilver,      sz  ); y += s;
    Lbl("St",  "State : " + stateStr,                cx, y, stateClr,       sz  ); y += s;
    Lbl("Mg",  StringFormat("Magic : %d",InpMagicNumber), cx, y, C'80,80,80', sz); y += s;
    Lbl("S1",  "────────────────────────",            cx, y, C'45,58,105'        ); y += s-3;
    Lbl("Bal", StringFormat("Balance : $%.2f", balance),  cx, y, clrSilver, sz  ); y += s;
    Lbl("Eq",  StringFormat("Equity  : $%.2f", equity),   cx, y, clrSilver, sz  ); y += s;
    Lbl("DPL", StringFormat("Day P/L : $%.2f", g_todayProfit), cx, y, cDay, sz  ); y += s;
    Lbl("S2",  "────────────────────────",            cx, y, C'45,58,105'        ); y += s-3;
    Lbl("FL",  StringFormat("Float   : $%.2f", floatPL),  cx, y, cFloat,    sz  ); y += s;
    Lbl("Spd", StringFormat("Spread  : %.0f pts", spread), cx, y, clrSilver, sz ); y += s;
    Lbl("Pos", StringFormat("Lệnh    : %d pos  %d pend",nPos,nPend), cx,y, clrSilver, sz); y += s;
    Lbl("Str", "Streak  : " + streakStr,              cx, y, cStreak,       sz  ); y += s;

    // ── PANEL 2: BỘ LỌC ──
    int p2Y = pY + p1H + InpPanelGap;
    int p2H = (InpPanel2H > 0) ? InpPanel2H
            : 10 + (s+2) + 5*s + 8;
    CreateBG("BG2", pX, p2Y, pW, p2H, C'17,21,32', C'65,90,160');

    y = p2Y + 8;
    Lbl("FT",  "═══  BỘ LỌC  ═══",                   cx, y, C'90,140,230', sz  ); y += s+2;
    Lbl("F1",  StringFormat("Trading Hour : %s  [%02d-%02d]",
        bTime ? "✓ OK " : "✗ OFF",
        InpStartHour, InpEndHour),                    cx, y, bTime ? fcOK : fcFail, sz); y += s;
    Lbl("F2",  StringFormat("Rollover     : %s  [%02d:%02d-%02d:%02d]",
        bRoll ? "✓ OK " : "✗ PAUSE",
        InpRolloverStartH, InpRolloverStartM,
        InpRolloverEndH,   InpRolloverEndM),           cx, y, bRoll ? fcOK : fcFail, sz); y += s;
    Lbl("F3",  StringFormat("Spread       : %s  [max %d]",
        bSprd ? "✓ OK " : "✗ HIGH",
        InpMaxSpreadPts),                              cx, y, bSprd ? fcOK : fcFail, sz); y += s;
    Lbl("F4",  StringFormat("ATR          : %s  [%.1f-%.1f]",
        bATR  ? "✓ OK " : "✗ BLOCK",
        InpATRMin, InpATRMax),                         cx, y, bATR ? fcOK : fcFail, sz); y += s;
    Lbl("F5",  StringFormat("ADX          : %s  [%.0f-%.0f]",
        bADX  ? "✓ OK " : "✗ BLOCK",
        InpADXMin, InpADXMax),                         cx, y, bADX ? fcOK : fcFail, sz); y += s;

    // ── PANEL 3: CẤU HÌNH TRAILING ──
    int p3Y = p2Y + p2H + InpPanelGap;
    int p3H = (InpPanel3H > 0) ? InpPanel3H
            : 10 + (s+2) + 8*s + 8;
    CreateBG("BG3", pX, p3Y, pW, p3H, C'14,19,28', C'50,70,130');

    y = p3Y + 8;
    Lbl("CT",  "═══  CẤU HÌNH  ═══",                 cx, y, C'90,140,230', sz  ); y += s+2;
    Lbl("C0",  StringFormat("Lot: %.2f  |  SL: %.1f  |  Dist: %.1f",
        InpLotSize, InpStopLoss, InpStraddleDist),     cx, y, clrSilver,    sz  ); y += s;
    Lbl("C1s", "Trailing Tiers:",                     cx, y, C'100,125,195', sz ); y += s;
    Lbl("C1",  StringFormat("T1 ≥%.1f → trail %.1f (dynamic)",
        InpT1Trigger, InpT1Trail),                    cx, y, clrSilver,     sz  ); y += s;
    Lbl("C2",  StringFormat("T2 ≥%.1f → lock %.1f",
        InpT2Trigger, InpT2Lock),                     cx, y, clrSilver,     sz  ); y += s;
    Lbl("C3",  StringFormat("T3 ≥%.1f → lock %.1f",
        InpT3Trigger, InpT3Lock),                     cx, y, clrSilver,     sz  ); y += s;
    Lbl("C4",  StringFormat("T4 ≥%.1f → lock %.1f",
        InpT4Trigger, InpT4Lock),                     cx, y, clrSilver,     sz  ); y += s;
    Lbl("C5",  StringFormat("T5 ≥%.1f → lock %.1f",
        InpT5Trigger, InpT5Lock),                     cx, y, clrSilver,     sz  ); y += s;
    Lbl("C6",  StringFormat("T6 ≥%.1f → lock %.1f",
        InpT6Trigger, InpT6Lock),                     cx, y, clrSilver,     sz  ); y += s;

    // ── PANEL 4: ĐIỀU KHIỂN ──
    int p4Y = p3Y + p3H + InpPanelGap;
    int p4H = (InpPanel4H > 0) ? InpPanel4H
            : 10 + (s+2) + 26 + 8;
    CreateBG("BG4", pX, p4Y, pW, p4H, C'17,21,32', C'65,90,160');

    y = p4Y + 8;
    Lbl("BT",  "═══  ĐIỀU KHIỂN  ═══",               cx, y, C'90,140,230', sz  ); y += s+2;
    CreateBtn("BtnCloseAll", "  Close All Orders", cx, y, pW - 16, 24,
              C'20,60,150', C'80,130,230');

    ChartRedraw(0);
}
