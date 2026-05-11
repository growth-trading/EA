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
#include <ChartObjects\ChartObjectsTxtControls.mqh>

//+------------------------------------------------------------------+
//|                                                                  |
//|  INPUT PARAMETERS SCHEMA                                         |
//|                                                                  |
//+------------------------------------------------------------------+

input group "=== GENERAL ==="
input int    InpMagicNumber    = 220426;          // Magic number nhan dien lenh cua bot
input string InpComment        = "BOT_XAUUSD_v2"; // Comment prefix cho tat ca order

input group "=== LOT SIZE ==="
input double InpLotSize        = 0.2;             // Lot co dinh cho moi lenh

input group "=== ENTRY ==="
input double InpStraddleDist   = 1.5;             // Khoang cach straddle tu gia hien tai (don vi gia)
input double InpStopLoss       = 3.0;             // SL cung tu entry (don vi gia)
input double InpTakeProfit     = 15.0;            // TP tu entry (don vi gia)

input group "=== TIME FILTER ==="
input int    InpStartHour      = 14;              // Gio bat dau trading (gio địa phương)
input int    InpEndHour        = 22;              // Gio ket thuc T2-T5
input int    InpEndMinute      = 30;              // Phut ket thuc T2-T5 -> 22:30
input int    InpFridayEndHour  = 21;              // Gio T6 dong som
input int    InpFridayEndMin   = 0;               // Phut T6 dong som -> 21:00
input int    InTimeGMT         = 0;               // Giờ địa phương  
input group "=== ROLLOVER FILTER ==="
input int    InpRolloverStartH = 3;               // Gio rollover bat dau
input int    InpRolloverStartM = 45;              // Phut rollover bat dau -> 03:45
input int    InpRolloverEndH   = 4;               // Gio rollover ket thuc
input int    InpRolloverEndM   = 30;              // Phut rollover ket thuc -> 04:30

input group "=== ATR FILTER ==="
input int              InpATRPeriod = 14;         // Chu ky ATR
input ENUM_TIMEFRAMES  InpATRTF     = PERIOD_M5;  // Khung thoi gian ATR
input double           InpATRMin    = 6.0;        // ATR min (don vi gia) de cho vao lenh
input double           InpATRMax    = 28.0;       // ATR max (don vi gia) de cho vao lenh

input group "=== ADX FILTER ==="
input int              InpADXPeriod = 14;         // Chu ky ADX
input ENUM_TIMEFRAMES  InpADXTF     = PERIOD_M15; // Khung thoi gian ADX
input double           InpADXMin    = 20.0;       // ADX phai > gia tri nay (exclusive)
input double           InpADXMax    = 55.0;       // ADX phai < gia tri nay (exclusive)

input group "=== SPREAD FILTER ==="
input int    InpMaxSpreadPts   = 600;              // Spread max (diem) = 0.6 gia

input group "=== RISK MANAGEMENT ==="
input double InpDailyLossPct   = 15.0;            // Gioi han lo ngay (%)
input int    InpMaxLossStreak  = 5;               // Max thua lien tiep truoc khi pause
input int    InpPauseMinutes   = 60;              // Thoi gian pause sau loss streak (phut)

input group "=== TRAILING SL ==="
input bool   InpEnableTrailing = true;            // Bat trailing SL
input double InpTrailTrigger   = 3.0;             // Muc lai de kich hoat trailing (don vi gia)

input group "=== TELEGRAM ==="
input bool   InpEnableTelegram = true;            // Master switch bat/tat Telegram
input string InpTelegramToken  = "";              // Bot token tu @BotFather
input string InpTelegramChatID = "1903206789";    // Chat ID nguoi nhan
input bool   InpNotifyEntry    = true;            // Notify khi dat straddle
input bool   InpNotifyExit     = true;            // Notify khi dong lenh
input bool   InpNotifyReset    = true;            // Notify khi reset
input bool   InpNotifyDaily    = true;            // Gui tong ket ngay luc 23:00
input group "=== SHIFT ==="
input int Shift = 400;
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
//|  PHAN 12 — STATE MACHINE (9 trang thai)                          |
//+------------------------------------------------------------------+

enum EA_STATE {
    STATE_IDLE,         // Khong co position/pending, cho filter PASS
    STATE_PLACING,      // Dang dat straddle
    STATE_WAITING,      // Ca 2 pending dang treo, cho fill
    STATE_POSITION,     // Co it nhat 1 position dang active
    STATE_RECOVERING,   // Vua dong lo, dang check dieu kien reset
    STATE_PAUSED,       // Loss streak 5, cho 60 phut
    STATE_DAILY_STOP,   // Cham 15% lo ngay, dung den nua dem
    STATE_EOD_CLOSE,    // Den gio EOD, dong tat ca
    STATE_DISCONNECTED  // Mat ket noi broker
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

// MQL5 trade objects
CTrade          g_trade;
CPositionInfo   g_posInfo;
COrderInfo      g_orderInfo;
CSymbolInfo     g_symInfo;

CChartObjectLabel lblIsInTradingHour, lblIsInRolloverWindow, lblIsSpreadOK, lblIsATROK, lblIsADXOK;

//+------------------------------------------------------------------+
//|                                                                  |
//|  PHAN 2.5 — PRICE / POINT CONVERSION                             |
//|  QUAN TRONG: XAUUSD - 1 gia = 100 diem (2-digit)                 |
//|                       1 gia = 1000 diem (3-digit)                |
//|  EA tu dong detect qua _Point                                    |
//|                                                                  |
//+------------------------------------------------------------------+

/// Chuyen doi gia -> diem (tu dong theo so digit cua symbol)
double PriceToPoints(double price_value) {
    return price_value / _Point;
}

/// Chuyen doi diem -> gia (tu dong theo so digit cua symbol)
double PointsToPrice(double points_value) {
    return points_value * _Point;
}

/// Lay stop level broker theo don vi GIA
double GetBrokerStopLevelPrice(){
   long stopPts = SymbolInfoInteger(_Symbol, SYMBOL_TRADE_STOPS_LEVEL);
   return PointsToPrice((double)stopPts);
}

//+------------------------------------------------------------------+
//|                                                                  |
//|  PHAN 9 — TELEGRAM NOTIFICATION SYSTEM                           |
//|                                                                  |
//+------------------------------------------------------------------+

/// URL encode UTF-8: ho tro ky tu tieng Viet (Phan 9.3)
string UrlEncode(string text) {
    string result = "";
    uchar  utf8[];
    // Convert string -> UTF-8 bytes
    StringToCharArray(text, utf8, 0, WHOLE_ARRAY, CP_UTF8);
    int uLen = ArraySize(utf8);
    for(int i = 0; i < uLen - 1; i++) {
        // bo null terminator cuoi
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

/// Gui message qua Telegram WebRequest (Phan 9.2)
bool SendTelegram(string text) {
    if(!InpEnableTelegram) return false;
    if(StringLen(InpTelegramToken) < 20) {
        return false;
    }
    // Rate limit: 1 msg/giay
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
    } if(httpCode == 429) {
        Sleep(1000);
        return SendTelegram(text);  // retry 1 lan
    }
    if(httpCode != 200){
        return false;
    }
    return true;
}

// --- 11 loai event notification (Phan 9.4) ---

void NotifyStart() {
    SendTelegram(StringFormat(
        "KHOI DONG: %s v%s\nSymbol: %s | Lot: %.2f\nSL: %.1f | TP: %.1f | Straddle: %.1f",
        EA_NAME, EA_VERSION, _Symbol, InpLotSize, InpStopLoss, InpTakeProfit, InpStraddleDist));
}

void NotifyStop(string reason) {
    SendTelegram(StringFormat("DUNG: %s\nLy do: %s", EA_NAME, reason));
}

void NotifyStraddleSetup(double buyPx, double sellPx) {
    if(!InpNotifyEntry) return;
    SendTelegram(StringFormat(
        "STRADDLE DAT OK\nBuy Stop: %.2f | Sell Stop: %.2f\nSL: %.1f | TP: %.1f",
        buyPx, sellPx, InpStopLoss, InpTakeProfit));
}

void NotifyCloseTP(ulong posId, double profit) {
    if(!InpNotifyExit) return;
    SendTelegram(StringFormat("TP HIT - Pos #%llu\nLai: +%.2f USD", posId, profit));
}

void NotifyCloseSL(ulong posId, double loss) {
    if(!InpNotifyExit) return;
    SendTelegram(StringFormat("SL HIT - Pos #%llu\nLo: %.2f USD", posId, loss));
}

void NotifyResetSetup(double buyPx, double sellPx) {
    if(!InpNotifyReset) return;
    SendTelegram(StringFormat("RESET Straddle\nBuy Stop: %.2f | Sell Stop: %.2f", buyPx, sellPx));
}

void NotifyPause(int streak) {
    SendTelegram(StringFormat(
        "EA TAM DUNG\n%d lenh thua lien tiep -> pause %d phut", streak, InpPauseMinutes));
}

void NotifyResume() {
    SendTelegram("EA TIEP TUC - Het thoi gian pause");
}

void NotifyDailyLimitHit(double pct) {
    SendTelegram(StringFormat(
        "DAILY LIMIT HIT\nDa lo %.1f%% equity ngay -> EA dung den nua dem", pct));
}

void NotifyDailySummary() {
    if(!InpNotifyDaily) return;
    double wr = (g_totalTrades > 0) ? (100.0 * g_totalWins / g_totalTrades) : 0.0;
    double pf = (g_grossLoss  != 0) ? (g_grossProfit / MathAbs(g_grossLoss)) : 0.0;
    double net = g_grossProfit + g_grossLoss;
    SendTelegram(StringFormat(
        "TONG KET NGAY\nTrades: %d | W: %d | L: %d\nWin Rate: %.1f%% | PF: %.2f\nNet PL: %.2f USD\nMax DD: %.2f USD\nFilter rejects: T=%lld R=%lld S=%lld ATR=%lld ADX=%lld",
        g_totalTrades, g_totalWins, g_totalLosses,
        wr, pf, net, g_maxDDIntraday,
        g_rejTime, g_rejRoll, g_rejSpread, g_rejATR, g_rejADX));
}

void NotifyErrorAlert(string msg) {
    SendTelegram("LOI NGHIEM TRONG\n" + msg);
}

//+------------------------------------------------------------------+
//|                                                                  |
//|  PHAN 6 — HE THONG LOC 5 TANG (Short-circuit)                    |
//|                                                                  |
//+------------------------------------------------------------------+

/// Filter 1: Khung thoi gian (Phan 6.1)
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

/// Filter 2: Rollover window (Phan 6.2)
bool IsInRolloverWindow() {
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    int curMin   = now.hour * 60 + now.min;
    int startMin = (InpRolloverStartH - InTimeGMT) * 60 + InpRolloverStartM;
    int endMin   = (InpRolloverEndH - InTimeGMT)  * 60 + InpRolloverEndM;

    return (curMin >= startMin && curMin <= endMin);
}

/// Filter 3: Spread (Phan 6.3)
bool IsSpreadOK() {
    int spread = (int)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD);
    return (spread <= InpMaxSpreadPts);
}

/// Filter 4: ATR bien dong (Phan 6.4)
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

/// Filter 5: ADX do manh xu huong (Phan 6.5)
double GetADX() {
    double buf[];
    // Buffer 0 = ADX main line (MODE_MAIN)
    if(CopyBuffer(g_handleADX, 0, 0, 1, buf) != 1) return 0.0;
    return buf[0];
}

bool IsADXOK() {
    double adx = GetADX();
    if(adx <= 0.0) return false;
    // Exclusive bounds: NGHIEM NGAT > min VA < max
    return (adx > InpADXMin && adx < InpADXMax);
}

/// Danh gia tat ca 5 filter theo thu tu, short-circuit khi fail (Phan 6)
bool AllFiltersPass() {
    if(!IsInTradingHour())     { g_rejTime++;   return false; }
    if(IsInRolloverWindow())   { g_rejRoll++;   return false; }
    if(!IsSpreadOK())          { g_rejSpread++; return false; }
    if(!IsATROK())             { g_rejATR++;    return false; }
    if(!IsADXOK())             { g_rejADX++;    return false; }
    return true;
}

//+------------------------------------------------------------------+
//|                                                                  |
//|  PHAN 4 — TRAILING STOP PHAN TANG (7 tang)                     |
//|                                                                  |
//+------------------------------------------------------------------+

/// Tinh offset SL moi tu entry theo 7 tang lai (Phan 4.2)
double CalculateTrailingLockOffset(double profit) {
    if(profit < 3.0)   return 0.0;            // Tier 0: chua kich hoat
    if(profit < 4.0)   return 2.0;            // Tier 1: lock +2.0 (67%)
    if(profit < 5.0)   return 2.5;            // Tier 2: lock +2.5 (62.5%)
    if(profit <= 10.0) return profit * 0.70;  // Tier 3: 70%
    if(profit <= 20.0) return profit * 0.80;  // Tier 4: 80%
    if(profit <= 30.0) return profit * 0.85;  // Tier 5: 85%
    return profit * 0.90;                     // Tier 6: 90%
}

/// Ap dung trailing cho 1 position (Phan 4.2, 4.3)
void ApplyTrailing(ulong ticket) {
    if(!InpEnableTrailing) return;
    if(!g_posInfo.SelectByTicket(ticket)) return;
    if(g_posInfo.Magic() != InpMagicNumber) return;
    if(g_posInfo.Symbol() != _Symbol) return;
    
    double entry = g_posInfo.PriceOpen();
    double curSL = g_posInfo.StopLoss();
    double curTP = g_posInfo.TakeProfit();
    ENUM_POSITION_TYPE pType = g_posInfo.PositionType();
    // Tinh loi nhuan noi theo don vi GIA
    double mktPx = (pType == POSITION_TYPE_BUY)
                    ? SymbolInfoDouble(_Symbol, SYMBOL_BID)
                    : SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double profit = (pType == POSITION_TYPE_BUY)
                    ? (mktPx - entry)
                    : (entry - mktPx);
    double offset = CalculateTrailingLockOffset(profit);
    if(offset == 0.0) return;
    double newSL = (pType == POSITION_TYPE_BUY)
                    ? NormalizeDouble(entry + offset, _Digits)
                    : NormalizeDouble(entry - offset, _Digits);
    // CHI modify khi SL moi TOT HON NGHIEM NGAT SL hien tai (tranh float error)
    bool shouldModify = false;
    if(pType == POSITION_TYPE_BUY  && newSL > curSL + _Point) shouldModify = true;
    if(pType == POSITION_TYPE_SELL && newSL < curSL - _Point) shouldModify = true;
    if(!shouldModify) return;
    // Rate limit: toi da 1 modify/giay
    if(TimeCurrent() - g_lastTrailTime < TRAIL_RATE_LIMIT_S) return;
    if(g_trade.PositionModify(ticket, newSL, curTP)) {
        
        g_lastTrailTime = TimeCurrent();
    }
}

/// Ap dung trailing cho tat ca position dang mo
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

/// Huy 1 pending order, co retry (Phan 3.4 — chong orphan)
bool CancelOrderSafe(ulong ticket) {
    for(int attempt = 0; attempt < 2; attempt++) {
        if(g_trade.OrderDelete(ticket)) return true;
        Sleep(RETRY_DELAY_MS);
    }
    NotifyErrorAlert(StringFormat("ORPHAN — Khong huy duoc order #%llu!", ticket));
    return false;
}

//+------------------------------------------------------------------+
//|                                                                  |
//|  PHAN 7.5 — MARGIN CHECK                                        |
//|                                                                  |
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
//|                                                                  |
//|  PHAN 8.5 — BROKER ERROR HANDLING                               |
//|                                                                  |
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
            NotifyErrorAlert(ctx + ": Tai khoan khong du margin!");
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
//|                                                                  |
//|  PHAN 3 — LOGIC VAO LENH (DAT STRADDLE) — ATOMIC               |
//|                                                                  |
//+------------------------------------------------------------------+

/// Dat straddle nguyen tu (Buy Stop + Sell Stop) (Phan 3.2, 3.4)
/// isReset: true = straddle reset sau SL (Phan 5)
bool PlaceStraddle(bool isReset, double resetAtPrice) {
    //--- Throttle chong spam (Phan 3.5) ---
    if(!isReset) {
        if(TimeCurrent() - g_lastStraddleTime < STRADDLE_THROTTLE_S)
            return false;
        
        if(TimeCurrent() - g_straddleMinStart < 60){
            g_straddleInMin++;
            if(g_straddleInMin > MAX_STRADDLE_PER_MIN) {
                NotifyErrorAlert("Phat hien spam straddle — tu dong pause");
                g_pauseUntil = TimeCurrent() + InpPauseMinutes * 60;
                g_state      = STATE_PAUSED;
                CancelAllPendingOrders();
                return false;
            }
        } else {
            g_straddleInMin  = 1;
            g_straddleMinStart = TimeCurrent();
        }
    }
    g_lastStraddleTime = TimeCurrent();

    //--- Refresh rates va validate tick (Phan 8.4) ---
    if(!g_symInfo.RefreshRates()) return false;
    double bid = g_symInfo.Bid();
    double ask = g_symInfo.Ask();
    if(bid <= 0 || ask <= 0 || ask <= bid) return false;

    //--- Tinh gia dat lenh (Phan 3.2) ---
    double stopLvlPx = GetBrokerStopLevelPrice();
    double minDist   = stopLvlPx * 1.1;  // +10% buffer (Phan 8.6)
    double buyPx, sellPx;
    if(isReset && resetAtPrice > 0.0) {
        buyPx  = resetAtPrice + InpStraddleDist;
        sellPx = resetAtPrice - InpStraddleDist;
        // Fallback: neu gia SL qua xa thi truong (Phan 5.3)
        
        if(MathAbs(buyPx - ask) < stopLvlPx || MathAbs(sellPx - bid) < stopLvlPx) {
            buyPx  = ask + InpStraddleDist;
            sellPx = bid - InpStraddleDist;
        }
    } else {
        buyPx  = ask + InpStraddleDist;
        sellPx = bid - InpStraddleDist;
    }
    
    // Adjust neu vi pham stop level (Phan 8.6)
    if(buyPx - ask < minDist) {
        buyPx = NormalizeDouble(ask + minDist, _Digits);
    }
    
    if(bid - sellPx < minDist) {
        sellPx = NormalizeDouble(bid - minDist, _Digits);
    }
    
    buyPx  = NormalizeDouble(buyPx,  _Digits);
    sellPx = NormalizeDouble(sellPx, _Digits);

    //--- SL va TP (server-side) (Phan 4.1) ---
    double sl_buy  = NormalizeDouble(buyPx  - InpStopLoss,   _Digits);
    double tp_buy  = NormalizeDouble(buyPx  + InpTakeProfit, _Digits);
    double sl_sell = NormalizeDouble(sellPx + InpStopLoss,   _Digits);
    double tp_sell = NormalizeDouble(sellPx - InpTakeProfit, _Digits);

    //--- Comment (Phan 3.3, 5.4) ---
    string sfx   = isReset ? "_R" : "";
    string cBuy  = InpComment + "_BUY"  + sfx;
    string cSell = InpComment + "_SELL" + sfx;

    //--- Trade setup (Phan 3.3) ---
    g_trade.SetExpertMagicNumber(InpMagicNumber);
    g_trade.SetDeviationInPoints(20);
    g_trade.SetTypeFilling(ORDER_FILLING_RETURN);

    //--- DAT BUY STOP ---
    bool  buyOK  = false;
    ulong buyTkt = 0;
    for(int att = 0; att < MAX_RETRY_COUNT; att++) {
        buyOK = g_trade.BuyStop(InpLotSize, buyPx, _Symbol, sl_buy, tp_buy,
                              ORDER_TIME_GTC, 0, cBuy);
        if(buyOK) { 
            buyTkt = g_trade.ResultOrder(); 
            break; 
        }

        if(!HandleRetcode(g_trade.ResultRetcode(), "BuyStop")) break;
    }

    if(!buyOK || buyTkt == 0) return false;

    //--- DAT SELL STOP ---
    bool  sellOK  = false;
    ulong sellTkt = 0;
    for(int att = 0; att < MAX_RETRY_COUNT; att++) {
        sellOK = g_trade.SellStop(InpLotSize, sellPx, _Symbol, sl_sell, tp_sell,
                                    ORDER_TIME_GTC, 0, cSell);
        if(sellOK) { 
            sellTkt = g_trade.ResultOrder(); 
            break; 
        }
        
        if(!HandleRetcode(g_trade.ResultRetcode(), "SellStop")) break;
    }

    //--- ATOMICITY: neu SellStop that bai, ROLLBACK BuyStop (Phan 3.4) ---
    if(!sellOK || sellTkt == 0) {
        if(!CancelOrderSafe(buyTkt))
            NotifyErrorAlert(StringFormat("ORPHAN BuyStop #%llu — can kiem tra thu cong!", buyTkt));
        return false;
    }

    //--- Luu tracking ---
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
//|                                                                  |
//|  PHAN 5 — LOGIC RESET                                           |
//|                                                                  |
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
//|                                                                  |
//|  PHAN 7 — QUAN LY RUI RO                                        |
//|                                                                  |
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

/// Cap nhat max drawdown intraday
void UpdateIntradayDD() {
    double eq = AccountInfoDouble(ACCOUNT_EQUITY);
    if(eq > g_peakEquityToday) g_peakEquityToday = eq;
    double dd = g_peakEquityToday - eq;
    if(dd > g_maxDDIntraday) g_maxDDIntraday = dd;
}

/// Kiem tra daily loss limit (Phan 7.1)
void CheckDailyLossLimit() {
    if(g_dailyLimitHit || g_dayStartEquity <= 0) return;
    
    double eq   = AccountInfoDouble(ACCOUNT_EQUITY);
    double totalDD    = g_dayStartEquity - eq;
    double totalDDPct = (totalDD / g_dayStartEquity) * 100.0;
    // Force-close neu total drawdown (da chot + floating) > 20% (Phan 7.1)
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

/// Cap nhat loss streak va stats sau khi trade dong (Phan 7.2)
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

/// Kiem tra va xu ly khi pause het han (Phan 7.2)
void CheckPauseExpiry() {
    if(g_state != STATE_PAUSED) return;
    if(g_pauseUntil > 0 && TimeCurrent() >= g_pauseUntil) {
        g_pauseUntil = 0;
        g_state      = STATE_IDLE;
        NotifyResume();
    }
}

/// Kiem tra EOD close (Phan 7.3)
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
//|                                                                  |
//|  PHAN 8 — SPECIAL CASES & ERROR HANDLING                        |
//|                                                                  |
//+------------------------------------------------------------------+

/// Kiem tra orphan order (Phan 8.2): sau ORPHAN_CHECK_S giay, huy leg con lai
void CheckOrphanOrder() {
    if(g_lastFillTime == 0) return;
    if(TimeCurrent() - g_lastFillTime < (datetime)ORPHAN_CHECK_S) return;
    // Tim pending order con lai cua EA ma khong phai pos da fill
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

/// Validate tick (Phan 8.4)
bool ValidateTick() {
    if(!g_symInfo.RefreshRates()) return false;
    if(g_symInfo.Bid() == 0.0 || g_symInfo.Ask() == 0.0) return false;
    if(g_symInfo.Ask() <= g_symInfo.Bid()) {
        return false;
    }
    return true;
}

//+------------------------------------------------------------------+
//|                                                                  |
//|  PHAN 11.13 — INPUT VALIDATION                                  |
//|                                                                  |
//+------------------------------------------------------------------+

bool ValidateInputs() {
    bool ok = true;
    double volMin = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
    double volMax = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
    if(InpLotSize < volMin || InpLotSize > volMax) {
      Print("INPUT ERROR: InpLotSize=", InpLotSize, " phai trong [", volMin, ", ", volMax, "]");
      ok = false;
    }
    if(InpStopLoss <= 0)
        { Print("INPUT ERROR: InpStopLoss phai > 0"); ok = false; }
    if(InpTakeProfit <= InpStopLoss)
        { Print("INPUT ERROR: InpTakeProfit phai > InpStopLoss (RR >= 1)"); ok = false; }
    if(InpStraddleDist <= 0)
        { Print("INPUT ERROR: InpStraddleDist phai > 0"); ok = false; }
    if(InpATRMin >= InpATRMax)
        { Print("INPUT ERROR: InpATRMin phai < InpATRMax"); ok = false; }
    if(InpADXMin >= InpADXMax || InpADXMin < 0 || InpADXMax > 100)
        { Print("INPUT ERROR: ADX range khong hop le"); ok = false; }
    if(InpDailyLossPct <= 0 || InpDailyLossPct >= 100)
        { Print("INPUT ERROR: InpDailyLossPct phai trong (0, 100)"); ok = false; }
    if(InpStartHour >= InpEndHour || InpStartHour < 0 || InpEndHour > 23)
        { Print("INPUT ERROR: StartHour/EndHour khong hop le"); ok = false; }
    if(InpEnableTelegram && StringLen(InpTelegramToken) < 40)
        { Print("INPUT WARN: Telegram bat nhung Token co ve khong du dai (< 40 ky tu)"); }
    return ok;
}

//+------------------------------------------------------------------+
//|  PHAN 8.7 — ACCOUNT TYPE VALIDATION                             |
//+------------------------------------------------------------------+

void ValidateAccountType() {
    long mode = AccountInfoInteger(ACCOUNT_TRADE_MODE);
    if(mode == ACCOUNT_TRADE_MODE_CONTEST)
        Print("CONTEST account — mot so tinh nang co the bi gioi han");
    else if(mode == ACCOUNT_TRADE_MODE_DEMO)
        Print("DEMO account — EA dang chay tren tai khoan demo");
    else
        Print("REAL account — EA dang chay tren tai khoan that");
}

//+------------------------------------------------------------------+
//|  PHAN 12.3 — STATE RECOVERY SAU RESTART                        |
//+------------------------------------------------------------------+

void RecoverState() {
    int posCnt = CountMyPositions();
    int ordCnt = CountMyPendingOrders();
    if(posCnt > 0)      g_state = STATE_POSITION;
    else if(ordCnt > 0) g_state = STATE_WAITING;
    else                g_state = STATE_IDLE;

    // Recover loss streak tu lich su hom nay
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
            else { streak = 0; break; }  // thang cuoi -> reset streak
        }
        g_lossStreak = streak;
    } else {
        g_lossStreak = 0;
    }
}

//+------------------------------------------------------------------+
//|  TIMER TASKS — DAILY SUMMARY & HOUSEKEEPING                     |
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
//|                                                                  |
//|  PHAN 12.2 — MAIN EVENT HANDLERS                                |
//|                                                                  |
//+------------------------------------------------------------------+

int OnInit() {
    //--- Validate inputs (Phan 11.13) ---
    if(!ValidateInputs())
        return INIT_PARAMETERS_INCORRECT;
    
    int x = (int)ChartGetInteger(0, CHART_WIDTH_IN_PIXELS) - Shift;

    if(!CreateLable(lblIsInTradingHour, "lblIsInTradingHour", "IsInTradingHour: ", x, 30))
        return(INIT_FAILED);
    if(!CreateLable(lblIsInRolloverWindow, "lblIsInRolloverWindow", "IsInRolloverWindow: ", x, 60))
        return(INIT_FAILED);
    if(!CreateLable(lblIsSpreadOK, "lblIsSpreadOK", "IsSpreadOK: ", x, 90))
        return(INIT_FAILED);
    if(!CreateLable(lblIsATROK, "lblIsATROK", "IsATROK: ", x, 120))
        return(INIT_FAILED);
    if(!CreateLable(lblIsADXOK, "lblIsADXOK", "lblIsADXOK: ", x, 150))
        return(INIT_FAILED);
        //--- Validate account type (Phan 8.7) ---
    ValidateAccountType();
    //--- Symbol info setup ---
    if(!g_symInfo.Name(_Symbol)) {
        Print("Loi khoi tao CSymbolInfo cho: ", _Symbol);
        return INIT_FAILED;
    }

    //--- Trade object setup ---
    g_trade.SetExpertMagicNumber(InpMagicNumber);
    g_trade.SetDeviationInPoints(20);

    //--- Khoi tao indicator handles (MOT LAN duy nhat — Phan 13.4) ---
    g_handleATR = iATR(_Symbol, InpATRTF, InpATRPeriod);
    if(g_handleATR == INVALID_HANDLE) {
        Print("iATR init that bai — kiem tra lai ATR period/timeframe");
        return INIT_FAILED;
    }
    g_handleADX = iADX(_Symbol, InpADXTF, InpADXPeriod);
    if(g_handleADX == INVALID_HANDLE) {
        Print("iADX init that bai — kiem tra lai ADX period/timeframe");
        return INIT_FAILED;
    }

    //--- Timer 1 giay (Phan 12.2, 13.6) ---
    EventSetTimer(1);

    //--- Equity baseline ---
    g_dayStartEquity  = AccountInfoDouble(ACCOUNT_EQUITY);
    g_peakEquityToday = g_dayStartEquity;
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    g_lastDayChecked = StringToTime(StringFormat("%04d.%02d.%02d", now.year, now.mon, now.day));

    //--- State recovery sau restart (Phan 12.3) ---
    RecoverState();

    //--- Notify Telegram (Event #1) ---
    NotifyStart();

    return INIT_SUCCEEDED;
}

//+------------------------------------------------------------------+
//| OnDeinit — Tat EA (Phan 12.2)                                   |
//+------------------------------------------------------------------+
void OnDeinit(const int reason) {
    //--- Release indicator handles (Phan 13.5) ---
    if(g_handleATR != INVALID_HANDLE) { IndicatorRelease(g_handleATR); g_handleATR = INVALID_HANDLE; }
    if(g_handleADX != INVALID_HANDLE) { IndicatorRelease(g_handleADX); g_handleADX = INVALID_HANDLE; }

    //--- Kill timer ---
    EventKillTimer();

    //--- Determine reason string ---
    string rsn = "Code " + IntegerToString(reason);
    switch(reason) {
        case REASON_REMOVE:     rsn = "EA bi xoa khoi chart"; break;
        case REASON_RECOMPILE:  rsn = "Recompile";           break;
        case REASON_CHARTCLOSE: rsn = "Chart dong";           break;
        case REASON_ACCOUNT:    rsn = "Doi account";          break;
        case REASON_INITFAILED: rsn = "Init that bai";        break;
        case REASON_CLOSE:      rsn = "Terminal dong";        break;
    }

    //--- Notify Telegram (Event #2) ---
    NotifyStop(rsn);
}

//+------------------------------------------------------------------+
//| OnTick — Vong lap chinh (Phan 12.4 Flowchart)                  |
//+------------------------------------------------------------------+
void OnTick(){
    //--- 1. Validate tick (Phan 8.4) ---
    if(!ValidateTick()) return;

    //--- 2. Kiem tra ket noi broker (Phan 8.3) ---
    bool connected = (bool)TerminalInfoInteger(TERMINAL_CONNECTED);
    if(!connected) {
        if(g_state != STATE_DISCONNECTED) {
            g_prevState      = g_state;
            g_state          = STATE_DISCONNECTED;
            g_disconnectStart = TimeCurrent();
        }
        ManageTrailing();  // SL/TP phia server van active, trailing van chay
        return;
    }
    if(g_state == STATE_DISCONNECTED) {
        g_state = g_prevState;
        RecoverState();
        g_disconnectStart = 0;
    }

    //--- 3. Kiem tra ngay moi (Phan 7.1) ---
    CheckNewDay();

    //--- 4. Cap nhat drawdown ---
    UpdateIntradayDD();

    //--- 5. EOD close? (Phan 7.3) ---
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

    //--- Trailing luon chay ke ca khi pause (neu co position) ---
    ManageTrailing();

    if(g_state == STATE_PAUSED) return;

    //--- 8. Kiem tra orphan order (Phan 8.2) ---
    CheckOrphanOrder();

    //--- 9. Co position hoac pending? -> khong overlap (Phan 3.1) ---
    if(CountMyPositions() > 0 || CountMyPendingOrders() > 0) {
        g_state = (CountMyPositions() > 0) ? STATE_POSITION : STATE_WAITING;
        return;
    }

    //--- 10. Buffer 15 phut truoc EOD -> khong entry moi (Phan 3.1) ---
    MqlDateTime now;
    TimeToStruct(TimeCurrent(), now);
    int curMin = now.hour * 60 + now.min;
    int eodMin = (now.day_of_week == 5)
                ? ((InpFridayEndHour - InTimeGMT) * 60 + InpFridayEndMin)
                : ((InpEndHour - InTimeGMT) * 60 + InpEndMinute);
    if(curMin >= eodMin - EOD_BUFFER_MIN) return;

    //--- 11. Check tat ca 5 filter (Phan 6) ---
    if(!AllFiltersPass()) { g_state = STATE_IDLE; return; }

    //--- 12. Margin check (Phan 7.5) ---
    if(!HasSufficientMargin()) return;

    //--- 13. Dat straddle (Phan 3) ---
    g_state = STATE_PLACING;
    PlaceStraddle(false, 0.0);
}

//+------------------------------------------------------------------+
//| OnTimer — Housekeeping moi giay (Phan 12.2, 13.6)              |
//+------------------------------------------------------------------+
void OnTimer() {
    CheckPauseExpiry();
    CheckDailySummary();

    //--- Alert neu mat ket noi > 5 phut co position (Phan 8.3) ---
    if(g_disconnectStart > 0 &&
        TimeCurrent() - g_disconnectStart > DISCONNECT_ALERT_S &&
        CountMyPositions() > 0) {
        NotifyErrorAlert(StringFormat("Mat ket noi > %d giay voi position dang mo!", DISCONNECT_ALERT_S));
        g_disconnectStart = TimeCurrent();  // reset timer de khong spam
    }

    lblIsInTradingHour.Description("IsInTradingHour: " + (IsInTradingHour() ? "True" : "False"));
    lblIsInRolloverWindow.Description("IsInRolloverWindow: " + (!IsInRolloverWindow() ? "True" : "False"));
    lblIsSpreadOK.Description("IsSpreadOK: " + (IsSpreadOK() ? "True" : "False"));
    lblIsATROK.Description("IsATROK: " + (IsATROK() ? "True" : "False"));
    lblIsADXOK.Description("IsADXOK: " + (IsADXOK() ? "True" : "False"));
}

//+------------------------------------------------------------------+
//| OnTradeTransaction — Phat hien fill, close, cap nhat stats      |
//| (Phan 4.4, 5, 7.2, 8.1, 8.2)                                   |
//+------------------------------------------------------------------+
void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest&     request,
                        const MqlTradeResult&      result){
    //--- Chi xu ly DEAL_ADD (giao dich da thuc thi) ---
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

    //=== DEAL IN: pending order kich hoat thanh position ===
    if(dealEntry == DEAL_ENTRY_IN) {

        g_state       = STATE_POSITION;
        g_lastFillTime = TimeCurrent();
        g_filledPosId  = posId;
        // Whipsaw check (Phan 8.1): neu ca 2 leg fill trong < 3 giay -> giu ca 2
        // CheckOrphanOrder() se xu ly huy leg con lai sau ORPHAN_CHECK_S giay
    }

    //=== DEAL OUT: position dong ===
    if(dealEntry == DEAL_ENTRY_OUT || dealEntry == DEAL_ENTRY_OUT_BY) {
        bool isManual    = (StringFind(comment, "close") >= 0 && StringFind(comment, "EOD") < 0);
        bool isEOD       = (StringFind(comment, "EOD") >= 0);
        bool isDailyStop = (StringFind(comment, "Daily") >= 0 || StringFind(comment, "Force") >= 0);

        //--- Manual close: log va skip reset (Phan 4.4) ---
        if(isManual && !isEOD && !isDailyStop) {
            UpdateAfterTrade(pl);
            g_state = STATE_IDLE;
            return;
        }

        //--- TP hit ---
        if(pl > 0 && !isEOD && !isDailyStop) {
            NotifyCloseTP(posId, pl);
            UpdateAfterTrade(pl);
            g_state = STATE_IDLE;
        }
        //--- SL hit (initial SL hoac trailing SL) ---
        else if(pl <= 0 && !isEOD && !isDailyStop) {
            NotifyCloseSL(posId, pl);

            // Phan biet trailing SL hit vs initial SL hit
            bool isInitialSL = (pl <= -(InpStopLoss * 0.8 * InpLotSize));  // ~gia tri lo SL ban dau
            if(isInitialSL) {
                UpdateAfterTrade(pl);
                g_state = STATE_RECOVERING;
                // Thu reset sau khi dam bao khong con position/order nao
                if(CountMyPositions() == 0 && CountMyPendingOrders() == 0){
                    TryReset(dealPrice);
                }
            } else {
                // Trailing SL hit -> reset streak counter theo spec (Phan 4.4)
                g_lossStreak = 0;
                g_totalTrades++;
                g_grossLoss   += pl;
                g_todayProfit += pl;
                g_state = STATE_IDLE;
            }
        }
        //--- EOD / Daily Stop close ---
        else {
            UpdateAfterTrade(pl);
            g_state = STATE_IDLE;
        }
    }
}

bool CreateLable(CChartObjectLabel &lable, string name, string des, int x, int y){
    // Tạo lable và thiết lập thuộc tính
    if(!lable.Create(0, name, 0, x, y))
        return false;

    lable.Description(des);
    lable.Color(clrWhite);
    lable.Font("Calibri");
    lable.FontSize(12);

    return true;
}