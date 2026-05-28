//+------------------------------------------------------------------+
//|                                        RichTradingBot.mq5        |
//|                        Rich Trading Bot v1.0 (MQL5)              |
//|                  Spec: Custumer/CLAUDE.md                        |
//+------------------------------------------------------------------+
#property copyright "Rich Trading Bot v1.0"
#property version   "1.00"
#property strict

#include <Trade\Trade.mqh>
#include <Trade\PositionInfo.mqh>

CTrade    Trade;

//+------------------------------------------------------------------+
//| ENUMS                                                            |
//+------------------------------------------------------------------+
enum ENUM_SIGNAL_MODE  { SIG_EMA, SIG_BZ_ZONE, SIG_ICHIMOKU, SIG_BB, SIG_SIMULATED };
enum ENUM_DIRECTION    { DIR_BOTH, DIR_ONLY_BUY, DIR_ONLY_SELL, DIR_EITHER };
enum ENUM_DCA_MODE     { DCA_STOP, DCA_STEP, DCA_STEP_TF };
enum ENUM_TRAIL_MODE   { TRAIL_BASKET, TRAIL_SINGLE };
enum ENUM_BOT_MODE     { MODE_AUTO, MODE_SEMI_AUTO };

//+------------------------------------------------------------------+
//| INPUT: BASE SETTINGS                                             |
//+------------------------------------------------------------------+
input group         "══════ CÀI ĐẶT CƠ BẢN ══════"; //
input  ENUM_BOT_MODE InpBotMode = MODE_AUTO;  // Chế độ: Tự động / Bán tự động
input  double  InpLotSize      = 0.01;    // Lots ban đầu
input  bool    InpUseTakeProfit= true;    // Dùng Take Profit (Use_TP)
input  bool    InpUseStopLoss  = false;   // Dùng Stop Loss (Use_SL)
input  bool    InpStealthMode  = false;   // Ẩn TP/SL trên chart (Stealth Mode)
input  int     InpOrderDelay   = 5;       // Độ trễ mở lệnh (giây)
input  ulong   InpMagic        = 202601;  // Magic Number
input  double  InpTP_Points    = 3000.0;  // TP mỗi lệnh (points)
input  double  InpSL_Points    = 0.0;     // SL mỗi lệnh (points, 0=tắt)

//+------------------------------------------------------------------+
//| INPUT: ENTRY SIGNAL                                              |
//+------------------------------------------------------------------+
input group         "══════ TÍN HIỆU VÀO LỆNH ══════"; //
input  ENUM_SIGNAL_MODE InpSignalMode = SIG_EMA;      // Chiến lược tín hiệu
input  ENUM_DIRECTION   InpDirection  = DIR_BOTH;     // Hướng giao dịch
input  ENUM_TIMEFRAMES  InpSignalTF   = PERIOD_H1;    // Khung thời gian tín hiệu

//+------------------------------------------------------------------+
//| INPUT: EMA                                                       |
//+------------------------------------------------------------------+
input group         "══════ EMA FILTER (34+89) ══════"; //
input  int     InpEMAFast  = 34;   // EMA nhanh
input  int     InpEMASlow  = 89;   // EMA chậm
input  double  InpEMAPullbackPts = 100.0; // Khoảng pullback về EMA34 (points)

//+------------------------------------------------------------------+
//| INPUT: BOLLINGER BANDS                                           |
//+------------------------------------------------------------------+
input group          "══════ BOLLINGER BANDS ══════"; //
input  int     InpBBPeriod = 20;   // BB Period
input  double  InpBBDev    = 2.0;  // BB Deviation

//+------------------------------------------------------------------+
//| INPUT: ICHIMOKU                                                  |
//+------------------------------------------------------------------+
input group         "══════ ICHIMOKU ══════"; //
input  int     InpIchiTenkan = 9;   // Tenkan-sen
input  int     InpIchiKijun  = 26;  // Kijun-sen
input  int     InpIchiSenkou = 52;  // Senkou Span B

//+------------------------------------------------------------------+
//| INPUT: GLOBAL FILTERS                                            |
//+------------------------------------------------------------------+
input group          "══════ BỘ LỌC CHUNG ══════"; //
input  int     InpMaxBuy   = 10;   // Số lệnh Buy tối đa
input  int     InpMaxSell  = 10;   // Số lệnh Sell tối đa

//+------------------------------------------------------------------+
//| INPUT: DCA (8 LEVELS)                                            |
//+------------------------------------------------------------------+
input group         "══════ DCA - CÀI ĐẶT CHUNG ══════"; //
input  ENUM_DCA_MODE InpDCAMode     = DCA_STEP; // DCA: Chế độ (áp dụng cho tất cả tầng)
input  bool          InpDCABuyEnable  = true;   // DCA: Bật DCA chiều Buy
input  bool          InpDCASellEnable = true;   // DCA: Bật DCA chiều Sell

input group         "══════ DCA - TẦNG 1 ══════"; //
input  double  InpDCA1Mult = 1.5;    // DCA T1: Hệ số Lot
input  int     InpDCA1Max  = 2;      // DCA T1: Max lệnh tổng tại tầng này
input  double  InpDCA1Dist = 1000.0; // DCA T1: Khoảng cách (points)
input  double  InpDCA1TP   = 500.0;  // DCA T1: TP (points)
input  double  InpDCA1SL   = 0.0;    // DCA T1: SL (points, 0=tắt)

input group         "══════ DCA - TẦNG 2 ══════"; //
input  double  InpDCA2Mult = 2.0;   // DCA T2: Hệ số Lot
input  int     InpDCA2Max  = 2;     // DCA T2: Max lệnh tổng tại tầng này
input  double  InpDCA2Dist = 1500.0; // DCA T2: Khoảng cách (points)
input  double  InpDCA2TP   = 500.0; // DCA T2: TP (points)
input  double  InpDCA2SL   = 0.0;   // DCA T2: SL (points, 0=tắt)

input group         "══════ DCA - TẦNG 3 ══════"; //
input  double  InpDCA3Mult = 2.5;   // DCA T3: Hệ số Lot
input  int     InpDCA3Max  = 2;     // DCA T3: Max lệnh tổng tại tầng này
input  double  InpDCA3Dist = 2000.0;// DCA T3: Khoảng cách (points)
input  double  InpDCA3TP   = 500.0; // DCA T3: TP (points)
input  double  InpDCA3SL   = 0.0;   // DCA T3: SL (points, 0=tắt)

input group         "══════ DCA - TẦNG 4 ══════"; //
input  double  InpDCA4Mult = 3.0;   // DCA T4: Hệ số Lot
input  int     InpDCA4Max  = 2;     // DCA T4: Max lệnh tổng tại tầng này
input  double  InpDCA4Dist = 2500.0; // DCA T4: Khoảng cách (points)
input  double  InpDCA4TP   = 500.0; // DCA T4: TP (points)
input  double  InpDCA4SL   = 0.0;   // DCA T4: SL (points, 0=tắt)

input group         "══════ DCA - TẦNG 5 ══════"; //
input  double  InpDCA5Mult = 3.5;  // DCA T5: Hệ số Lot
input  int     InpDCA5Max  = 2;    // DCA T5: Max lệnh tổng tại tầng này
input  double  InpDCA5Dist = 3000.0; // DCA T5: Khoảng cách (points)
input  double  InpDCA5TP   = 500.0; // DCA T5: TP (points)
input  double  InpDCA5SL   = 0.0;   // DCA T5: SL (points, 0=tắt)

input group         "══════ DCA - TẦNG 6 ══════"; //
input  double  InpDCA6Mult = 4.0; // DCA T6: Hệ số Lot
input  int     InpDCA6Max  = 2;   // DCA T6: Max lệnh tổng tại tầng này
input  double  InpDCA6Dist = 3500.0; // DCA T6: Khoảng cách (points)
input  double  InpDCA6TP   = 500.0; // DCA T6: TP (points)
input  double  InpDCA6SL   = 0.0;  // DCA T6: SL (points, 0=tắt)

input group         "══════ DCA - TẦNG 7 ══════"; //
input  double  InpDCA7Mult = 5.0; // DCA T7: Hệ số Lot
input  int     InpDCA7Max  = 2;  // DCA T7: Max lệnh tổng tại tầng này
input  double  InpDCA7Dist = 4000.0; // DCA T7: Khoảng cách (points)
input  double  InpDCA7TP   = 500.0; // DCA T7: TP (points)
input  double  InpDCA7SL   = 0.0;  // DCA T7: SL (points, 0=tắt)

input group         "══════ DCA - TẦNG 8 ══════"; //
input  double  InpDCA8Mult = 6.0; // DCA T8: Hệ số Lot
input  int     InpDCA8Max  = 1; // DCA T8: Max lệnh tổng tại tầng này
input  double  InpDCA8Dist = 5000.0; // DCA T8: Khoảng cách (points)
input  double  InpDCA8TP   = 500.0; // DCA T8: TP (points)
input  double  InpDCA8SL   = 0.0; // DCA T8: SL (points, 0=tắt)

//+------------------------------------------------------------------+
//| INPUT: PYRAMIDING (NHỒI DƯƠNG)                                   |
//+------------------------------------------------------------------+
input group         "══════ NHỒI DƯƠNG (PYRA) ══════"; //
input  ENUM_DCA_MODE InpPyraMode      = DCA_STEP; // PYRA: Chế độ (áp dụng cho tất cả tầng)
input  bool          InpPyraBuyEnable  = true;    // PYRA: Bật nhồi chiều Buy
input  bool          InpPyraSellEnable = true;    // PYRA: Bật nhồi chiều Sell

input group         "══════ PYRA - TẦNG 1 ══════"; //
input  double  InpPyra1Mult = 1.0;    // PYRA T1: Hệ số Lot
input  int     InpPyra1Max  = 2;      // PYRA T1: Max lệnh tổng tại tầng này
input  double  InpPyra1Dist = 500.0;  // PYRA T1: Khoảng cách (points)
input  double  InpPyra1TP   = 3000.0; // PYRA T1: TP (points)
input  double  InpPyra1SL   = 0.0;    // PYRA T1: SL (points, 0=tắt)

input group         "══════ PYRA - TẦNG 2 ══════"; //
input  double  InpPyra2Mult = 1.0;    // PYRA T2: Hệ số Lot
input  int     InpPyra2Max  = 2;      // PYRA T2: Max lệnh tổng tại tầng này
input  double  InpPyra2Dist = 500.0;  // PYRA T2: Khoảng cách (points)
input  double  InpPyra2TP   = 3000.0; // PYRA T2: TP (points)
input  double  InpPyra2SL   = 0.0;    // PYRA T2: SL (points, 0=tắt)

input group         "══════ PYRA - TẦNG 3 ══════"; //
input  double  InpPyra3Mult = 1.0;    // PYRA T3: Hệ số Lot
input  int     InpPyra3Max  = 2;      // PYRA T3: Max lệnh tổng tại tầng này
input  double  InpPyra3Dist = 500.0;  // PYRA T3: Khoảng cách (points)
input  double  InpPyra3TP   = 3000.0; // PYRA T3: TP (points)
input  double  InpPyra3SL   = 0.0;    // PYRA T3: SL (points, 0=tắt)

input group         "══════ PYRA - TẦNG 4 ══════"; //
input  double  InpPyra4Mult = 1.0;    // PYRA T4: Hệ số Lot
input  int     InpPyra4Max  = 2;      // PYRA T4: Max lệnh tổng tại tầng này
input  double  InpPyra4Dist = 500.0;  // PYRA T4: Khoảng cách (points)
input  double  InpPyra4TP   = 3000.0; // PYRA T4: TP (points)
input  double  InpPyra4SL   = 0.0;    // PYRA T4: SL (points, 0=tắt)

input group         "══════ PYRA - TẦNG 5 ══════"; //
input  double  InpPyra5Mult = 1.0;    // PYRA T5: Hệ số Lot
input  int     InpPyra5Max  = 2;      // PYRA T5: Max lệnh tổng tại tầng này
input  double  InpPyra5Dist = 500.0;  // PYRA T5: Khoảng cách (points)
input  double  InpPyra5TP   = 3000.0; // PYRA T5: TP (points)
input  double  InpPyra5SL   = 0.0;    // PYRA T5: SL (points, 0=tắt)

input group         "══════ PYRA - TẦNG 6 ══════"; //
input  double  InpPyra6Mult = 1.0;    // PYRA T6: Hệ số Lot
input  int     InpPyra6Max  = 2;      // PYRA T6: Max lệnh tổng tại tầng này
input  double  InpPyra6Dist = 500.0;  // PYRA T6: Khoảng cách (points)
input  double  InpPyra6TP   = 3000.0; // PYRA T6: TP (points)
input  double  InpPyra6SL   = 0.0;    // PYRA T6: SL (points, 0=tắt)

input group         "══════ PYRA - TẦNG 7 ══════"; //
input  double  InpPyra7Mult = 1.0;    // PYRA T7: Hệ số Lot
input  int     InpPyra7Max  = 2;      // PYRA T7: Max lệnh tổng tại tầng này
input  double  InpPyra7Dist = 500.0;  // PYRA T7: Khoảng cách (points)
input  double  InpPyra7TP   = 3000.0; // PYRA T7: TP (points)
input  double  InpPyra7SL   = 0.0;    // PYRA T7: SL (points, 0=tắt)

input group         "══════ PYRA - TẦNG 8 ══════"; //
input  double  InpPyra8Mult = 1.0;    // PYRA T8: Hệ số Lot
input  int     InpPyra8Max  = 1;      // PYRA T8: Max lệnh tổng tại tầng này
input  double  InpPyra8Dist = 500.0;  // PYRA T8: Khoảng cách (points)
input  double  InpPyra8TP   = 3000.0; // PYRA T8: TP (points)
input  double  InpPyra8SL   = 0.0;    // PYRA T8: SL (points, 0=tắt)

//+------------------------------------------------------------------+
//| INPUT: ORDER TRIMMING                                            |
//+------------------------------------------------------------------+
input group         "══════ TỈA LỆNH (TRIMMING) ══════"; //
input  bool    InpTrimEnable     = false;  // Bật Tỉa Lệnh
input  bool    InpTrimHedge      = false;  // Tỉa chéo (Hedging mode)
input  int     InpTrimTrigger    = 5;      // Kích hoạt khi số lệnh >= X
input  double  InpTrimTarget     = 10.0;   // Mục tiêu lợi nhuận sau tỉa ($)
input  int     InpTrimMaxLoss    = 1;      // Số lệnh âm tối đa cần tỉa mỗi lần
input  int     InpTrimMaxWin     = 1;      // Số lệnh dương tối đa dùng để tỉa (Hedge)
input  bool    InpPartialTrim    = false;  // Bật Tỉa Một Phần
input  double  InpPartialTrimDD  = 20.0;   // Kích hoạt khi DD% >
input  bool    InpTrimByDayProfit= false;  // Tỉa theo Lãi Ngày

//+------------------------------------------------------------------+
//| INPUT: TRAILING STOP                                             |
//+------------------------------------------------------------------+
input group         "══════ TRAILING STOP ══════"; //
input  bool          InpTrailEnable   = false;        // Bật Trailing
input  ENUM_TRAIL_MODE InpTrailMode   = TRAIL_BASKET; // Basket hoặc Đơn lẻ
input  int           InpTrailMinOrds  = 1;            // Số lệnh tối thiểu kích hoạt
input  double        InpTrailActivate = 500.0;        // Points kích hoạt Trail
input  double        InpTrailStep     = 200.0;        // Bước nhảy SL (points)
input  double        InpTrailInit     = 300.0;        // SL đầu tiên cách giá (points)
input  bool          InpTrailShowLine = true;         // Vẽ đường Trail

//+------------------------------------------------------------------+
//| INPUT: EXIT LOGIC                                                |
//+------------------------------------------------------------------+
input group         "══════ ĐÓNG LỆNH TỔNG ══════"; //
input  double  InpCloseProfit  = 0.0;  // Chốt lời khi tổng lãi đạt ($, 0=tắt)
input  double  InpCloseLoss    = 0.0;  // Cắt lỗ khi tổng lỗ đạt ($, 0=tắt)
input  double  InpClosePerPips = 0.0;  // Đóng từng lệnh khi đạt (points, 0=tắt)

//+------------------------------------------------------------------+
//| GLOBAL STATE                                                     |
//+------------------------------------------------------------------+
int      hEMAFast   = INVALID_HANDLE;
int      hEMASlow   = INVALID_HANDLE;
int      hBB        = INVALID_HANDLE;
int      hIchi      = INVALID_HANDLE;

// DCA config arrays (index 0-7 = level 1-8)
ENUM_DCA_MODE DCA_Mode[8];
double        DCA_Mult[8];
int           DCA_MaxOrd[8];
double        DCA_Dist[8];
double        DCA_TP[8];
double        DCA_SL[8];

// Pyramiding config arrays (index 0-7 = level 1-8)
ENUM_DCA_MODE PYRA_Mode[8];
double        PYRA_Mult[8];
int           PYRA_MaxOrd[8];
double        PYRA_Dist[8];
double        PYRA_TP[8];
double        PYRA_SL[8];

datetime LastOrderTime  = 0;
datetime LastEntryTime  = 0;
double   InitBalance    = 0.0;
double   MaxDrawdownPct = 0.0;
double   DayProfit      = 0.0;
int      LastDay        = -1;

// Basket trail levels
double   TrailBuy  = 0.0;
double   TrailSell = 0.0;

// GUI prefix
const string GUI = "RTB_";

//+------------------------------------------------------------------+
//| UTILITY FUNCTIONS                                                |
//+------------------------------------------------------------------+

int CountPos(int posType) {
    int n = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if((int)PositionGetInteger(POSITION_TYPE) == posType) n++;
    }
    return n;
}

int CountBuy()  { return CountPos(POSITION_TYPE_BUY);  }
int CountSell() { return CountPos(POSITION_TYPE_SELL); }
int CountAll()  { return CountBuy() + CountSell(); }

// Đếm pyramiding orders theo comment prefix "RTP|" — restart-safe, không lẫn với DCA
int CountPyra(int posType) {
    int n = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if((int)PositionGetInteger(POSITION_TYPE) != posType) continue;
        if(StringFind(PositionGetString(POSITION_COMMENT), "RTP|") == 0) n++;
    }
    return n;
}

double FloatProfit(int posType = -1) {
    double p = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if(posType >= 0 && (int)PositionGetInteger(POSITION_TYPE) != posType) continue;
        p += PositionGetDouble(POSITION_PROFIT) + PositionGetDouble(POSITION_SWAP);
    }
    return p;
}

double TotalLot(int posType) {
    double lot = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if((int)PositionGetInteger(POSITION_TYPE) != posType) continue;
        lot += PositionGetDouble(POSITION_VOLUME);
    }
    return lot;
}

// Last opened price for a direction (most recently opened position)
double LastOpenPrice(int posType) {
    double   price = 0;
    datetime latest = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if((int)PositionGetInteger(POSITION_TYPE) != posType) continue;
        datetime t = (datetime)PositionGetInteger(POSITION_TIME);
        if(t > latest) { latest = t; price = PositionGetDouble(POSITION_PRICE_OPEN); }
    }
    return price;
}

// Weighted average open price
double AvgOpenPrice(int posType) {
    double totalLot = 0, totalCost = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if((int)PositionGetInteger(POSITION_TYPE) != posType) continue;
        double lot   = PositionGetDouble(POSITION_VOLUME);
        double price = PositionGetDouble(POSITION_PRICE_OPEN);
        totalLot  += lot;
        totalCost += lot * price;
    }
    return (totalLot > 0) ? totalCost / totalLot : 0;
}

// Ticket of position with worst (most negative) floating profit
ulong WorstTicket() {
    ulong  tk_worst = 0;
    double worst    = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        double p = PositionGetDouble(POSITION_PROFIT);
        if(p < worst) { worst = p; tk_worst = tk; }
    }
    return tk_worst;
}

// Best (most positive) ticket
ulong BestTicket() {
    ulong  tk_best = 0;
    double best    = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        double p = PositionGetDouble(POSITION_PROFIT);
        if(p > best) { best = p; tk_best = tk; }
    }
    return tk_best;
}

void CloseAll(int posType = -1) {
    ulong tickets[];
    int   count = 0;
    ArrayResize(tickets, PositionsTotal());
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if(posType >= 0 && (int)PositionGetInteger(POSITION_TYPE) != posType) continue;
        tickets[count++] = tk;
    }
    Trade.SetAsyncMode(true);
    for(int i = 0; i < count; i++)
        Trade.PositionClose(tickets[i]);
    Trade.SetAsyncMode(false);
}

void CloseAllProfit() {
    ulong tickets[];
    int   count = 0;
    ArrayResize(tickets, PositionsTotal());
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if(PositionGetDouble(POSITION_PROFIT) + PositionGetDouble(POSITION_SWAP) > 0)
            tickets[count++] = tk;
    }
    Trade.SetAsyncMode(true);
    for(int i = 0; i < count; i++)
        Trade.PositionClose(tickets[i]);
    Trade.SetAsyncMode(false);
}

void CloseAllLoss() {
    ulong tickets[];
    int   count = 0;
    ArrayResize(tickets, PositionsTotal());
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        if(PositionGetDouble(POSITION_PROFIT) + PositionGetDouble(POSITION_SWAP) < 0)
            tickets[count++] = tk;
    }
    Trade.SetAsyncMode(true);
    for(int i = 0; i < count; i++)
        Trade.PositionClose(tickets[i]);
    Trade.SetAsyncMode(false);
}

double NormLot(double lot) {
    double minL  = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
    double maxL  = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
    double stepL = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
    lot = MathRound(lot / stepL) * stepL;
    return MathMax(minL, MathMin(maxL, lot));
}

//+------------------------------------------------------------------+
//| OPEN ORDER                                                       |
//| isDCA=false : TP/SL server chỉ đặt khi InpUseTakeProfit/SL=true  |
//| isDCA=true  : luôn đặt server TP/SL nếu giá trị > 0 (bỏ qua flag)|
//+------------------------------------------------------------------+
// isDCA=true  : luôn đặt server TP/SL nếu > 0, comment "RTB|tp|sl"
// isPyra=true : luôn đặt server TP/SL nếu > 0, comment "RTP|tp|sl" (phân biệt với DCA)
// cả hai false: theo InpUseTakeProfit/SL, comment "RTB|0|0"
bool OpenOrder(int ordType, double lot, double tp_pts = 0, double sl_pts = 0,
               bool isDCA = false, bool isPyra = false) {
    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);

    double price, tp = 0, sl = 0;

    bool autoExit = (isDCA || isPyra);
    bool applyTP  = autoExit ? (tp_pts > 0) : (InpUseTakeProfit && tp_pts > 0);
    bool applySL  = autoExit ? (sl_pts > 0) : (InpUseStopLoss   && sl_pts > 0);

    if(ordType == ORDER_TYPE_BUY) {
        price = ask;
        if(applyTP && !InpStealthMode)
            tp = NormalizeDouble(price + tp_pts * point, _Digits);
        if(applySL && !InpStealthMode)
            sl = NormalizeDouble(price - sl_pts * point, _Digits);
    } else {
        price = bid;
        if(applyTP && !InpStealthMode)
            tp = NormalizeDouble(price - tp_pts * point, _Digits);
        if(applySL && !InpStealthMode)
            sl = NormalizeDouble(price + sl_pts * point, _Digits);
    }

    string comment;
    if(isPyra)     comment = StringFormat("RTP|%.0f|%.0f", tp_pts, sl_pts);
    else if(isDCA) comment = StringFormat("RTB|%.0f|%.0f", tp_pts, sl_pts);
    else           comment = "RTB|0|0";

    lot = NormLot(lot);
    bool ok;
    if(ordType == ORDER_TYPE_BUY)
        ok = Trade.Buy(lot, _Symbol, price, sl, tp, comment);
    else
        ok = Trade.Sell(lot, _Symbol, price, sl, tp, comment);

    if(ok) {
        LastOrderTime = TimeCurrent();
        string tag = isPyra ? " [PYRA]" : (isDCA ? " [DCA]" : " [Entry]");
        Print("RTB: Open ", (ordType == ORDER_TYPE_BUY ? "BUY" : "SELL"),
              " lot=", lot, " tp=", tp, " sl=", sl, tag);
    } else {
        Print("RTB: OpenOrder FAILED type=", ordType, " err=", GetLastError());
    }
    return ok;
}

//+------------------------------------------------------------------+
//| ENTRY SIGNALS                                                    |
//+------------------------------------------------------------------+

// Returns +1 = BUY, -1 = SELL, 0 = no signal
int SignalEMA() {
    double fast[], slow[];
    ArraySetAsSeries(fast, true);
    ArraySetAsSeries(slow, true);
    if(CopyBuffer(hEMAFast, 0, 0, 3, fast) < 3) return 0;
    if(CopyBuffer(hEMASlow, 0, 0, 3, slow) < 3) return 0;

    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
    double price = (double)iClose(_Symbol, InpSignalTF, 0);

    // Golden / Death cross
    bool crossUp   = fast[2] < slow[2] && fast[1] > slow[1];
    bool crossDown = fast[2] > slow[2] && fast[1] < slow[1];

    // Pullback to EMA34: price within InpEMAPullbackPts of EMA34
    bool trendUp   = fast[0] > slow[0];
    bool trendDown = fast[0] < slow[0];
    bool pullBuy   = trendUp   && MathAbs(price - fast[0]) <= InpEMAPullbackPts * point;
    bool pullSell  = trendDown && MathAbs(price - fast[0]) <= InpEMAPullbackPts * point;

    if(crossUp  || pullBuy)  return  1;
    if(crossDown || pullSell) return -1;
    return 0;
}

int SignalBZZone() {
    MqlRates r[];
    ArraySetAsSeries(r, true);
    // Read 3 closed candles (index 1-3)
    if(CopyRates(_Symbol, InpSignalTF, 1, 3, r) < 3) return 0;

    bool allGreen = r[0].close > r[0].open && r[1].close > r[1].open && r[2].close > r[2].open;
    bool allRed   = r[0].close < r[0].open && r[1].close < r[1].open && r[2].close < r[2].open;

    if(allGreen) return  1;
    if(allRed)   return -1;
    return 0; // Gray = no signal
}

int SignalIchimoku() {
    double tenkan[], kijun[], spanA[], spanB[], chikou[];
    ArraySetAsSeries(tenkan,  true);
    ArraySetAsSeries(kijun,   true);
    ArraySetAsSeries(spanA,   true);
    ArraySetAsSeries(spanB,   true);
    ArraySetAsSeries(chikou,  true);

    if(CopyBuffer(hIchi, 0, 0, 3, tenkan)  < 3) return 0;
    if(CopyBuffer(hIchi, 1, 0, 3, kijun)   < 3) return 0;
    if(CopyBuffer(hIchi, 2, 0, 3, spanA)   < 3) return 0;
    if(CopyBuffer(hIchi, 3, 0, 3, spanB)   < 3) return 0;
    if(CopyBuffer(hIchi, 4, 0, 3, chikou)  < 3) return 0;

    double price    = (double)iClose(_Symbol, InpSignalTF, 0);
    double pastPrice= (double)iClose(_Symbol, InpSignalTF, InpIchiKijun); // Chikou lag
    double kumoTop  = MathMax(spanA[0], spanB[0]);
    double kumoBot  = MathMin(spanA[0], spanB[0]);

    // Tenkan cross Kijun (golden = buy, death = sell) on bar[2] vs [1]
    bool tkCrossUp  = tenkan[2] < kijun[2] && tenkan[1] > kijun[1];
    bool tkCrossDown= tenkan[2] > kijun[2] && tenkan[1] < kijun[1];

    bool buyOK  = price > kumoTop  && tkCrossUp   && chikou[0] > pastPrice;
    bool sellOK = price < kumoBot  && tkCrossDown  && chikou[0] < pastPrice;

    if(buyOK)  return  1;
    if(sellOK) return -1;
    return 0;
}

int SignalBB() {
    double upper[], lower[], mid[];
    ArraySetAsSeries(upper, true);
    ArraySetAsSeries(lower, true);
    ArraySetAsSeries(mid,   true);
    if(CopyBuffer(hBB, 0, 0, 2, mid)   < 2) return 0;
    if(CopyBuffer(hBB, 1, 0, 2, upper) < 2) return 0;
    if(CopyBuffer(hBB, 2, 0, 2, lower) < 2) return 0;

    double closeBar1 = (double)iClose(_Symbol, InpSignalTF, 1);

    if(closeBar1 <= lower[1]) return  1;  // Touch/breach lower band → BUY
    if(closeBar1 >= upper[1]) return -1;  // Touch/breach upper band → SELL
    return 0;
}

int SignalSimulated() {
    if(InpDirection == DIR_ONLY_BUY)  return  1;
    if(InpDirection == DIR_ONLY_SELL) return -1;
    return 2; // Both/Either: signal = 2 → mở cả BUY lẫn SELL
}

int GetSignal() {
    int sig = 0;
    switch(InpSignalMode) {
        case SIG_EMA:       sig = SignalEMA();       break;
        case SIG_BZ_ZONE:   sig = SignalBZZone();    break;
        case SIG_ICHIMOKU:  sig = SignalIchimoku();  break;
        case SIG_BB:        sig = SignalBB();        break;
        case SIG_SIMULATED: sig = SignalSimulated(); break;
    }
    // SIG_SIMULATED đã tự filter theo InpDirection — chỉ apply cho các strategy khác
    if(InpSignalMode != SIG_SIMULATED) {
        if(InpDirection == DIR_ONLY_BUY  && sig < 0) return 0;
        if(InpDirection == DIR_ONLY_SELL && sig > 0) return 0;
    }
    return sig;
}

//+------------------------------------------------------------------+
//| INITIAL ENTRY (OnTick)                                           |
//+------------------------------------------------------------------+
void TryOpenBuy() {
    if(CountBuy() >= InpMaxBuy) return;
    if(CountBuy() > 0) return;   // đã có lệnh → DCA xử lý
    if(OpenOrder(ORDER_TYPE_BUY, InpLotSize, InpTP_Points, InpSL_Points))
        LastEntryTime = TimeCurrent();
}

void TryOpenSell() {
    if(CountSell() >= InpMaxSell) return;
    if(CountSell() > 0) return;
    if(OpenOrder(ORDER_TYPE_SELL, InpLotSize, InpTP_Points, InpSL_Points))
        LastEntryTime = TimeCurrent();
}

void CheckEntry() {
    if(InpBotMode == MODE_SEMI_AUTO) return;
    if(TimeCurrent() - LastEntryTime < InpOrderDelay) return;

    int sig = GetSignal();
    if(sig == 0) return;

    if(sig == 2) {
        // Simulated Both/Either: mở BUY và SELL độc lập
        // Mỗi hướng tự quản lý DCA/Trail/Trim riêng
        TryOpenBuy();
        TryOpenSell();
        return;
    }

    if(sig > 0) TryOpenBuy();
    if(sig < 0) TryOpenSell();
}

//+------------------------------------------------------------------+
//| DCA LOGIC                                                        |
//+------------------------------------------------------------------+
void CheckDCA(int posType) {
    if(posType == POSITION_TYPE_BUY  && !InpDCABuyEnable)  return;
    if(posType == POSITION_TYPE_SELL && !InpDCASellEnable) return;

    int count = CountPos(posType);
    if(count == 0) return;

    int dcaCount = count - 1; // số lệnh DCA đã mở (không tính lệnh gốc)

    int lvl = -1;
    int cumulative = 0;
    for(int i = 0; i < 8; i++) {
        int nextCum = cumulative + DCA_MaxOrd[i];
        if(dcaCount < nextCum) { lvl = i; break; }
        cumulative = nextCum;
    }
    if(lvl < 0) return; // Đã hết 8 tầng
    if(DCA_Mode[lvl] == DCA_STOP) return;

    int maxOrds = (posType == POSITION_TYPE_BUY) ? InpMaxBuy : InpMaxSell;
    if(count >= maxOrds) return;

    double lastPrice = LastOpenPrice(posType);
    if(lastPrice == 0) return;

    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
    double dist  = DCA_Dist[lvl] * point;

    bool trigger = false;
    if(posType == POSITION_TYPE_BUY)
        trigger = (lastPrice - bid) >= dist;  // Price fell below entry by dist
    else
        trigger = (ask - lastPrice) >= dist;  // Price rose above entry by dist

    if(!trigger) return;

    // Step+TF: require same-direction signal confirmation
    if(DCA_Mode[lvl] == DCA_STEP_TF) {
        int sig = GetSignal();
        if(posType == POSITION_TYPE_BUY  && sig != 1)  return;
        if(posType == POSITION_TYPE_SELL && sig != -1) return;
    }

    if(TimeCurrent() - LastOrderTime < InpOrderDelay) return;

    double lot = NormLot(InpLotSize * DCA_Mult[lvl]);
    int    ord = (posType == POSITION_TYPE_BUY) ? ORDER_TYPE_BUY : ORDER_TYPE_SELL;
    Print("RTB: DCA level ", lvl+1, " triggered. count=", count, " dcaOrds=", dcaCount);
    OpenOrder(ord, lot, DCA_TP[lvl], DCA_SL[lvl], true);
}

//+------------------------------------------------------------------+
//| PYRAMIDING (NHỒI DƯƠNG)                                          |
//+------------------------------------------------------------------+
void CheckPyramiding(int posType) {
    if(posType == POSITION_TYPE_BUY  && !InpPyraBuyEnable)  return;
    if(posType == POSITION_TYPE_SELL && !InpPyraSellEnable) return;

    int count = CountPos(posType);
    if(count == 0) return;

    // Đọc từ broker qua comment "RTP|" — restart-safe, không lẫn với DCA orders
    int pyraCount = CountPyra(posType);

    int lvl = -1;
    int cumulative = 0;
    for(int i = 0; i < 8; i++) {
        int nextCum = cumulative + PYRA_MaxOrd[i];
        if(pyraCount < nextCum) { lvl = i; break; }
        cumulative = nextCum;
    }
    if(lvl < 0) return;
    if(PYRA_Mode[lvl] == DCA_STOP) return;

    int maxOrds = (posType == POSITION_TYPE_BUY) ? InpMaxBuy : InpMaxSell;
    if(count >= maxOrds) return;

    double lastPrice = LastOpenPrice(posType);
    if(lastPrice == 0) return;

    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
    double dist  = PYRA_Dist[lvl] * point;

    bool inProfit = false;
    if(posType == POSITION_TYPE_BUY)
        inProfit = (bid - lastPrice) >= dist;
    else
        inProfit = (lastPrice - ask) >= dist;

    if(!inProfit) return;

    if(PYRA_Mode[lvl] == DCA_STEP_TF) {
        int sig = GetSignal();
        if(posType == POSITION_TYPE_BUY  && sig != 1)  return;
        if(posType == POSITION_TYPE_SELL && sig != -1) return;
    }

    if(TimeCurrent() - LastOrderTime < InpOrderDelay) return;

    double lot = NormLot(InpLotSize * PYRA_Mult[lvl]);
    int    ord = (posType == POSITION_TYPE_BUY) ? ORDER_TYPE_BUY : ORDER_TYPE_SELL;

    // Khi PYRA_TP=0 và PYRA_SL=0: fallback về isPyra=false, dùng InpUseTakeProfit + InpTP_Points
    // như lệnh gốc (comment "RTB|0|0", section 2b xử lý Stealth)
    bool   hasTierExit = (PYRA_TP[lvl] > 0 || PYRA_SL[lvl] > 0);
    double openTP      = hasTierExit ? PYRA_TP[lvl] : InpTP_Points;
    double openSL      = hasTierExit ? PYRA_SL[lvl] : InpSL_Points;

    Print("RTB: Pyramiding level ", lvl+1, " triggered. pyraCount=", pyraCount);
    OpenOrder(ord, lot, openTP, openSL, false, hasTierExit);
}

//+------------------------------------------------------------------+
//| ORDER TRIMMING                                                   |
//+------------------------------------------------------------------+
void CheckTrimming() {
    if(!InpTrimEnable) return;
    if(CountAll() < InpTrimTrigger) return;

    double balance = AccountInfoDouble(ACCOUNT_BALANCE);
    double equity  = AccountInfoDouble(ACCOUNT_EQUITY);

    // Partial trim: by drawdown%
    if(InpPartialTrim && balance > 0) {
        double ddPct = (balance - equity) / balance * 100.0;
        if(ddPct > InpPartialTrimDD) {
            int closed = 0;
            for(int n = 0; n < InpTrimMaxLoss; n++) {
                if(CountAll() < InpTrimTrigger) break;
                ulong tk = WorstTicket();
                if(tk == 0) break;
                Trade.PositionClose(tk);
                closed++;
            }
            if(closed > 0) {
                Print("RTB: Partial Trim DD=", ddPct, "% closed=", closed);
                return;
            }
        }
    }

    // Trim by day profit: if today's closed profit > |worst floating loss|
    if(InpTrimByDayProfit) {
        int closed = 0;
        for(int n = 0; n < InpTrimMaxLoss; n++) {
            ulong worstTk = WorstTicket();
            if(worstTk == 0 || !PositionSelectByTicket(worstTk)) break;
            double worstP = PositionGetDouble(POSITION_PROFIT);
            if(DayProfit > MathAbs(worstP) && DayProfit > 0) {
                Trade.PositionClose(worstTk);
                closed++;
            } else break;
        }
        if(closed > 0) {
            Print("RTB: Trim by DayProfit=", DayProfit, " closed=", closed);
            return;
        }
    }

    // Hedging trim: close pairs of best+worst up to min(MaxWin, MaxLoss)
    if(InpTrimHedge) {
        int pairs  = MathMin(InpTrimMaxLoss, InpTrimMaxWin);
        int closed = 0;
        for(int n = 0; n < pairs; n++) {
            ulong worstTk = WorstTicket();
            ulong bestTk  = BestTicket();
            if(worstTk == 0 || bestTk == 0 || worstTk == bestTk) break;
            if(!PositionSelectByTicket(bestTk))  break;
            double bestP = PositionGetDouble(POSITION_PROFIT);
            if(!PositionSelectByTicket(worstTk)) break;
            double worstP = PositionGetDouble(POSITION_PROFIT);
            if(bestP + worstP >= InpTrimTarget) {
                Trade.PositionClose(worstTk);
                Trade.PositionClose(bestTk);
                closed++;
            } else break;
        }
        if(closed > 0) {
            Print("RTB: Hedge trim, pairs=", closed);
            return;
        }
    }

    // Same-direction trim: use aggregate floating profit vs target
    if(InpTrimTarget > 0) {
        int closed = 0;
        for(int n = 0; n < InpTrimMaxLoss; n++) {
            double totalProfit = FloatProfit();
            if(totalProfit < InpTrimTarget) break;
            ulong worstTk = WorstTicket();
            if(worstTk == 0 || !PositionSelectByTicket(worstTk)) break;
            double worstP = PositionGetDouble(POSITION_PROFIT);
            if((totalProfit + worstP) >= InpTrimTarget) {
                Trade.PositionClose(worstTk);
                closed++;
            } else break;
        }
        if(closed > 0)
            Print("RTB: Trim target met, closed=", closed);
    }
}

//+------------------------------------------------------------------+
//| TRAILING STOP                                                    |
//+------------------------------------------------------------------+
void ApplyTrailToPos(ulong tk, int posType, double newSL) {
    if(!PositionSelectByTicket(tk)) return;
    double curSL = PositionGetDouble(POSITION_SL);
    double curTP = PositionGetDouble(POSITION_TP);
    bool   move  = false;
    if(posType == POSITION_TYPE_BUY  && (curSL == 0 || newSL > curSL)) move = true;
    if(posType == POSITION_TYPE_SELL && (curSL == 0 || newSL < curSL)) move = true;
    if(move) Trade.PositionModify(tk, NormalizeDouble(newSL, _Digits), curTP);
}

void CheckTrailing() {
    if(!InpTrailEnable) return;
    if(CountAll() < InpTrailMinOrds) return;

    double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);

    if(InpTrailMode == TRAIL_BASKET) {
        // --- BUY BASKET ---
        if(CountBuy() > 0) {
            double avgBuy = AvgOpenPrice(POSITION_TYPE_BUY);
            if(bid - avgBuy >= InpTrailActivate * point) {
                double newSL = bid - InpTrailInit * point;
                bool   moved = false;
                if(TrailBuy == 0 || newSL >= TrailBuy + InpTrailStep * point) {
                    TrailBuy = newSL;
                    moved = true;
                }
                if(moved) {
                    for(int i = PositionsTotal()-1; i >= 0; i--) {
                        ulong tk = PositionGetTicket(i);
                        if(!PositionSelectByTicket(tk)) continue;
                        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
                        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
                        if((int)PositionGetInteger(POSITION_TYPE) != POSITION_TYPE_BUY) continue;
                        ApplyTrailToPos(tk, POSITION_TYPE_BUY, TrailBuy);
                    }
                }
            }
        } else { TrailBuy = 0; }

        // --- SELL BASKET ---
        if(CountSell() > 0) {
            double avgSell = AvgOpenPrice(POSITION_TYPE_SELL);
            if(avgSell - ask >= InpTrailActivate * point) {
                double newSL = ask + InpTrailInit * point;
                bool   moved = false;
                if(TrailSell == 0 || newSL <= TrailSell - InpTrailStep * point) {
                    TrailSell = newSL;
                    moved = true;
                }
                if(moved) {
                    for(int i = PositionsTotal()-1; i >= 0; i--) {
                        ulong tk = PositionGetTicket(i);
                        if(!PositionSelectByTicket(tk)) continue;
                        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
                        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
                        if((int)PositionGetInteger(POSITION_TYPE) != POSITION_TYPE_SELL) continue;
                        ApplyTrailToPos(tk, POSITION_TYPE_SELL, TrailSell);
                    }
                }
            }
        } else { TrailSell = 0; }

        // Draw lines
        if(InpTrailShowLine) {
            if(TrailBuy  > 0) DrawHLine("TrailBuy",  TrailBuy,  clrLimeGreen);
            if(TrailSell > 0) DrawHLine("TrailSell", TrailSell, clrTomato);
        }

    } else {
        // --- SINGLE TRAILING PER POSITION ---
        for(int i = PositionsTotal()-1; i >= 0; i--) {
            ulong tk = PositionGetTicket(i);
            if(!PositionSelectByTicket(tk)) continue;
            if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
            if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;

            int    pt        = (int)PositionGetInteger(POSITION_TYPE);
            double openPrice = PositionGetDouble(POSITION_PRICE_OPEN);

            if(pt == POSITION_TYPE_BUY) {
                double profitPts = (bid - openPrice) / point;
                if(profitPts >= InpTrailActivate) {
                    double newSL = bid - InpTrailInit * point;
                    double curSL = PositionGetDouble(POSITION_SL);
                    if(curSL == 0 || newSL >= curSL + InpTrailStep * point)
                        ApplyTrailToPos(tk, POSITION_TYPE_BUY, newSL);
                }
            } else {
                double profitPts = (openPrice - ask) / point;
                if(profitPts >= InpTrailActivate) {
                    double newSL = ask + InpTrailInit * point;
                    double curSL = PositionGetDouble(POSITION_SL);
                    if(curSL == 0 || newSL <= curSL - InpTrailStep * point)
                        ApplyTrailToPos(tk, POSITION_TYPE_SELL, newSL);
                }
            }
        }
    }
}

//+------------------------------------------------------------------+
//| EXIT LOGIC                                                       |
//+------------------------------------------------------------------+
void CheckExit() {
    // 1. Per-position exit by pips target
    if(InpClosePerPips > 0) {
        double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
        double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
        double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);

        for(int i = PositionsTotal()-1; i >= 0; i--) {
            ulong tk = PositionGetTicket(i);
            if(!PositionSelectByTicket(tk)) continue;
            if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
            if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;

            int    pt    = (int)PositionGetInteger(POSITION_TYPE);
            double opn   = PositionGetDouble(POSITION_PRICE_OPEN);
            double ppts  = (pt == POSITION_TYPE_BUY) ? (bid - opn) / point : (opn - ask) / point;

            if(ppts >= InpClosePerPips)
                Trade.PositionClose(tk);
        }
    }

    // 2a. DCA TP/SL — luôn chạy (không cần Stealth Mode)
    //     Đóng lệnh DCA đúng TP/SL của từng tầng dù Use_TP = false
    {
        double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
        double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
        double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);

        for(int i = PositionsTotal()-1; i >= 0; i--) {
            ulong tk = PositionGetTicket(i);
            if(!PositionSelectByTicket(tk)) continue;
            if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
            if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;

            string cmt = PositionGetString(POSITION_COMMENT);
            // Xử lý DCA ("RTB|tp|sl") và Pyramiding ("RTP|tp|sl") — bỏ qua "RTB|0|0" (lệnh gốc)
            bool isDCAcmt  = (StringFind(cmt, "RTB|") == 0);
            bool isPyracmt = (StringFind(cmt, "RTP|") == 0);
            if(!isDCAcmt && !isPyracmt) continue;
            string parts[];
            if(StringSplit(cmt, '|', parts) < 3) continue;
            double useTP = StringToDouble(parts[1]);
            double useSL = StringToDouble(parts[2]);
            if(useTP == 0 && useSL == 0) continue; // lệnh gốc "RTB|0|0" hoặc pyra fallback, bỏ qua

            int    pt  = (int)PositionGetInteger(POSITION_TYPE);
            double opn = PositionGetDouble(POSITION_PRICE_OPEN);

            if(pt == POSITION_TYPE_BUY) {
                if(useTP > 0 && bid >= opn + useTP * point) { Trade.PositionClose(tk); continue; }
                if(useSL > 0 && bid <= opn - useSL * point)   Trade.PositionClose(tk);
            } else {
                if(useTP > 0 && ask <= opn - useTP * point) { Trade.PositionClose(tk); continue; }
                if(useSL > 0 && ask >= opn + useSL * point)   Trade.PositionClose(tk);
            }
        }
    }

    // 2b. Stealth TP/SL — lệnh gốc, chỉ chạy khi Stealth Mode bật
    if(InpStealthMode) {
        double ask   = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
        double bid   = SymbolInfoDouble(_Symbol, SYMBOL_BID);
        double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);

        for(int i = PositionsTotal()-1; i >= 0; i--) {
            ulong tk = PositionGetTicket(i);
            if(!PositionSelectByTicket(tk)) continue;
            if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
            if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;

            // Chỉ xử lý lệnh gốc (comment "RTB|0|0")
            string cmt = PositionGetString(POSITION_COMMENT);
            if(cmt != "RTB|0|0") continue;

            int    pt  = (int)PositionGetInteger(POSITION_TYPE);
            double opn = PositionGetDouble(POSITION_PRICE_OPEN);

            // Stealth Mode thay thế hoàn toàn server TP/SL — không phụ thuộc InpUseTakeProfit/SL
            if(pt == POSITION_TYPE_BUY) {
                if(InpTP_Points > 0 && bid >= opn + InpTP_Points * point)
                    { Trade.PositionClose(tk); continue; }
                if(InpSL_Points > 0 && bid <= opn - InpSL_Points * point)
                    Trade.PositionClose(tk);
            } else {
                if(InpTP_Points > 0 && ask <= opn - InpTP_Points * point)
                    { Trade.PositionClose(tk); continue; }
                if(InpSL_Points > 0 && ask >= opn + InpSL_Points * point)
                    Trade.PositionClose(tk);
            }
        }
    }

    // 3. Basket total profit target
    if(InpCloseProfit > 0) {
        if(FloatProfit() >= InpCloseProfit) {
            Print("RTB: CloseProfit target reached. Closing all.");
            CloseAll();
            return;
        }
    }

    // 4. Basket total loss cut
    if(InpCloseLoss > 0) {
        if(FloatProfit() <= -InpCloseLoss) {
            Print("RTB: CloseLoss limit hit. Closing all.");
            CloseAll();
            return;
        }
    }
}

//+------------------------------------------------------------------+
//| DAY PROFIT TRACKING                                              |
//+------------------------------------------------------------------+
void UpdateDayProfit() {
    MqlDateTime dt;
    TimeToStruct(TimeCurrent(), dt);
    if(dt.day != LastDay) {
        DayProfit = 0;
        LastDay   = dt.day;
    }

    datetime dayStart = StringToTime(StringFormat("%04d.%02d.%02d 00:00:00",
                         dt.year, dt.mon, dt.day));
    if(!HistorySelect(dayStart, TimeCurrent())) return;

    double closed = 0;
    for(int i = 0; i < HistoryDealsTotal(); i++) {
        ulong dTk = HistoryDealGetTicket(i);
        if(HistoryDealGetString(dTk, DEAL_SYMBOL) != _Symbol) continue;
        ENUM_DEAL_ENTRY de = (ENUM_DEAL_ENTRY)HistoryDealGetInteger(dTk, DEAL_ENTRY);
        if(de == DEAL_ENTRY_OUT || de == DEAL_ENTRY_OUT_BY)
            closed += HistoryDealGetDouble(dTk, DEAL_PROFIT) +
                      HistoryDealGetDouble(dTk, DEAL_SWAP);
    }
    DayProfit = closed;
}

//+------------------------------------------------------------------+
//| GUI                                                              |
//+------------------------------------------------------------------+
struct PeriodStats { double pips, profit, gain, lot; };

PeriodStats GetPeriodStats(datetime from, datetime to) {
    PeriodStats s;
    s.pips = s.profit = s.gain = s.lot = 0;
    if(!HistorySelect(from, to)) return s;
    double tickVal = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
    for(int i = 0; i < HistoryDealsTotal(); i++) {
        ulong dk = HistoryDealGetTicket(i);
        if(HistoryDealGetString(dk, DEAL_SYMBOL) != _Symbol) continue;
        ENUM_DEAL_ENTRY de = (ENUM_DEAL_ENTRY)HistoryDealGetInteger(dk, DEAL_ENTRY);
        if(de != DEAL_ENTRY_OUT && de != DEAL_ENTRY_OUT_BY) continue;
        double dp = HistoryDealGetDouble(dk, DEAL_PROFIT) + HistoryDealGetDouble(dk, DEAL_SWAP);
        double dv = HistoryDealGetDouble(dk, DEAL_VOLUME);
        s.profit += dp;
        s.lot    += dv;
    }
    if(tickVal > 0 && s.lot > 0) s.pips = s.profit / (s.lot * tickVal);
    s.gain = (InitBalance > 0) ? s.profit / InitBalance * 100.0 : 0;
    return s;
}

void CreateBtn(string name, string text, int x, int y, int w, int h, color bgClr, color borderClr = clrSilver) {
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0) {
        ObjectCreate(0, obj, OBJ_BUTTON, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,     CORNER_LEFT_UPPER);
        ObjectSetInteger(0, obj, OBJPROP_XDISTANCE,  x);
        ObjectSetInteger(0, obj, OBJPROP_YDISTANCE,  y);
        ObjectSetInteger(0, obj, OBJPROP_XSIZE,      w);
        ObjectSetInteger(0, obj, OBJPROP_YSIZE,      h);
        ObjectSetString(0,  obj, OBJPROP_FONT,       "Consolas");
        ObjectSetInteger(0, obj, OBJPROP_FONTSIZE,   8);
        ObjectSetInteger(0, obj, OBJPROP_BACK,       false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE, false);
    }
    ObjectSetString(0,  obj, OBJPROP_TEXT,         text);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,        clrWhite);
    ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,      bgClr);
    ObjectSetInteger(0, obj, OBJPROP_BORDER_COLOR, borderClr);
    ObjectSetInteger(0, obj, OBJPROP_STATE,        false);
}

void DrawHLine(string name, double price, color clr) {
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0)
        ObjectCreate(0, obj, OBJ_HLINE, 0, 0, price);
    ObjectSetDouble(0,  obj, OBJPROP_PRICE, price);
    ObjectSetInteger(0, obj, OBJPROP_COLOR, clr);
    ObjectSetInteger(0, obj, OBJPROP_STYLE, STYLE_DASH);
    ObjectSetInteger(0, obj, OBJPROP_WIDTH, 1);
}

void CreateRect(string name, int lx, int ly, int lw, int lh, color bg) {
    string obj = GUI + name;
    if(ObjectFind(0, obj) >= 0) return;
    ObjectCreate(0, obj, OBJ_RECTANGLE_LABEL, 0, 0, 0);
    ObjectSetInteger(0, obj, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
    ObjectSetInteger(0, obj, OBJPROP_XDISTANCE,   lx);
    ObjectSetInteger(0, obj, OBJPROP_YDISTANCE,   ly);
    ObjectSetInteger(0, obj, OBJPROP_XSIZE,       lw);
    ObjectSetInteger(0, obj, OBJPROP_YSIZE,       lh);
    ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,     bg);
    ObjectSetInteger(0, obj, OBJPROP_BORDER_TYPE, BORDER_FLAT);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,       bg);
    ObjectSetInteger(0, obj, OBJPROP_WIDTH,       0);
    ObjectSetInteger(0, obj, OBJPROP_BACK,        false);
    ObjectSetInteger(0, obj, OBJPROP_SELECTABLE,  false);
}

void Lbl(string name, string text, int x, int y, color clr = clrSilver, int sz = 9) {
    string obj = GUI + name;
    if(ObjectFind(0, obj) < 0) {
        ObjectCreate(0, obj, OBJ_LABEL, 0, 0, 0);
        ObjectSetInteger(0, obj, OBJPROP_CORNER,    CORNER_LEFT_UPPER);
        ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x);
        ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
        ObjectSetString(0,  obj, OBJPROP_FONT, "Consolas");
        ObjectSetInteger(0, obj, OBJPROP_BACK, false);
        ObjectSetInteger(0, obj, OBJPROP_SELECTABLE, false);
    }
    ObjectSetString(0,  obj, OBJPROP_TEXT,     text);
    ObjectSetInteger(0, obj, OBJPROP_COLOR,    clr);
    ObjectSetInteger(0, obj, OBJPROP_FONTSIZE, sz);
}

void UpdateGUI() {
    double balance   = AccountInfoDouble(ACCOUNT_BALANCE);
    double equity    = AccountInfoDouble(ACCOUNT_EQUITY);
    double spread    = (double)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD);
    double totalProfit = 0, buyProfit = 0, sellProfit = 0;
    int    nBuy = 0, nSell = 0;
    double lotBuy = 0, lotSell = 0;
    for(int i = PositionsTotal()-1; i >= 0; i--) {
        ulong tk = PositionGetTicket(i);
        if(!PositionSelectByTicket(tk)) continue;
        if(PositionGetInteger(POSITION_MAGIC) != (long)InpMagic) continue;
        if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
        double p   = PositionGetDouble(POSITION_PROFIT) + PositionGetDouble(POSITION_SWAP);
        double lot = PositionGetDouble(POSITION_VOLUME);
        int    pt  = (int)PositionGetInteger(POSITION_TYPE);
        totalProfit += p;
        if(pt == POSITION_TYPE_BUY)  { buyProfit  += p; nBuy++;  lotBuy  += lot; }
        else                         { sellProfit += p; nSell++; lotSell += lot; }
    }
    double pnlPct  = (InitBalance > 0) ? totalProfit / InitBalance * 100.0 : 0;
    double ddPct   = (balance > 0 && equity < balance) ? (balance - equity) / balance * 100.0 : 0;
    if(ddPct > MaxDrawdownPct) MaxDrawdownPct = ddPct;

    string sigName = "";
    switch(InpSignalMode) {
        case SIG_EMA:       sigName = "EMA 34+89";      break;
        case SIG_BZ_ZONE:   sigName = "BZ Zone";        break;
        case SIG_ICHIMOKU:  sigName = "Ichimoku";       break;
        case SIG_BB:        sigName = "Bollinger Band"; break;
        case SIG_SIMULATED: sigName = "Simulated";      break;
    }

    string dirName = "";
    color  dirClr  = clrSilver;
    switch(InpDirection) {
        case DIR_BOTH:      dirName = "▲▼ Both";     dirClr = clrDodgerBlue; break;
        case DIR_ONLY_BUY:  dirName = "▲  Buy Only"; dirClr = clrLimeGreen;  break;
        case DIR_ONLY_SELL: dirName = "▼  Sell Only"; dirClr = clrTomato;    break;
        case DIR_EITHER:    dirName = "▲▼ Either";   dirClr = clrDodgerBlue; break;
    }

    MqlDateTime dt;
    TimeToStruct(TimeLocal(), dt);
    string tStr = StringFormat("%04d/%02d/%02d  %02d:%02d:%02d",
                   dt.year, dt.mon, dt.day, dt.hour, dt.min, dt.sec);

    color cProfit = (totalProfit >= 0) ? clrLimeGreen : clrTomato;
    color cBuyP   = (buyProfit   >= 0) ? clrLimeGreen : clrTomato;
    color cSellP  = (sellProfit  >= 0) ? clrLimeGreen : clrTomato;
    color cDayP   = (DayProfit   >= 0) ? clrLimeGreen : clrTomato;

    // ── PANEL 1: THÔNG TIN ──
    string bg = GUI + "BG";
    if(ObjectFind(0, bg) < 0) {
        ObjectCreate(0, bg, OBJ_RECTANGLE_LABEL, 0, 0, 0);
        ObjectSetInteger(0, bg, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
        ObjectSetInteger(0, bg, OBJPROP_XDISTANCE,   5);
        ObjectSetInteger(0, bg, OBJPROP_YDISTANCE,   18);
        ObjectSetInteger(0, bg, OBJPROP_XSIZE,       252);
        ObjectSetInteger(0, bg, OBJPROP_YSIZE,       346);
        ObjectSetInteger(0, bg, OBJPROP_BGCOLOR,     C'14,17,26');
        ObjectSetInteger(0, bg, OBJPROP_BORDER_TYPE, BORDER_FLAT);
        ObjectSetInteger(0, bg, OBJPROP_COLOR,       C'50,65,120');
        ObjectSetInteger(0, bg, OBJPROP_WIDTH,       1);
        ObjectSetInteger(0, bg, OBJPROP_BACK,        false);
        ObjectSetInteger(0, bg, OBJPROP_SELECTABLE,  false);
    }

    // ── PANEL 2: ĐIỀU KHIỂN ──
    string bg2 = GUI + "BG2";
    if(ObjectFind(0, bg2) < 0) {
        ObjectCreate(0, bg2, OBJ_RECTANGLE_LABEL, 0, 0, 0);
        ObjectSetInteger(0, bg2, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
        ObjectSetInteger(0, bg2, OBJPROP_XDISTANCE,   5);
        ObjectSetInteger(0, bg2, OBJPROP_YDISTANCE,   378);
        ObjectSetInteger(0, bg2, OBJPROP_XSIZE,       252);
        ObjectSetInteger(0, bg2, OBJPROP_YSIZE,       110);
        ObjectSetInteger(0, bg2, OBJPROP_BGCOLOR,     C'17,21,32');
        ObjectSetInteger(0, bg2, OBJPROP_BORDER_TYPE, BORDER_FLAT);
        ObjectSetInteger(0, bg2, OBJPROP_COLOR,       C'65,90,160');
        ObjectSetInteger(0, bg2, OBJPROP_WIDTH,       1);
        ObjectSetInteger(0, bg2, OBJPROP_BACK,        false);
        ObjectSetInteger(0, bg2, OBJPROP_SELECTABLE,  false);
    }

    // ── PANEL 3: THỐNG KÊ ──
    string bg3 = GUI + "BG3";
    if(ObjectFind(0, bg3) < 0) {
        ObjectCreate(0, bg3, OBJ_RECTANGLE_LABEL, 0, 0, 0);
        ObjectSetInteger(0, bg3, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
        ObjectSetInteger(0, bg3, OBJPROP_XDISTANCE,   5);
        ObjectSetInteger(0, bg3, OBJPROP_YDISTANCE,   518);
        ObjectSetInteger(0, bg3, OBJPROP_XSIZE,       252);
        ObjectSetInteger(0, bg3, OBJPROP_YSIZE,       115);
        ObjectSetInteger(0, bg3, OBJPROP_BGCOLOR,     C'14,19,28');
        ObjectSetInteger(0, bg3, OBJPROP_BORDER_TYPE, BORDER_FLAT);
        ObjectSetInteger(0, bg3, OBJPROP_COLOR,       C'50,70,130');
        ObjectSetInteger(0, bg3, OBJPROP_WIDTH,       1);
        ObjectSetInteger(0, bg3, OBJPROP_BACK,        false);
        ObjectSetInteger(0, bg3, OBJPROP_SELECTABLE,  false);
    }

    // ── NỘI DUNG PANEL 1 ──
    int x = 12, y = 23, s = 16;
    Lbl("T",    " RICH TRADING BOT  v1.0",   x, y, C'80,160,255', 10); y += s+2;
    Lbl("L0",   "────────────────────────",   x, y, C'45,58,105'  );    y += s-2;
    Lbl("Tim",  "Time   : " + tStr,           x, y, clrSilver     );    y += s;
    Lbl("Sig",  "Signal : " + sigName,        x, y, clrYellow     );    y += s;
    string modeName = (InpBotMode == MODE_SEMI_AUTO) ? "Ban Tu Dong" : "Tu Dong";
    color  modeClr  = (InpBotMode == MODE_SEMI_AUTO) ? clrOrange    : clrLimeGreen;
    Lbl("Mod",  "Mode   : " + modeName,       x, y, modeClr       );    y += s;
    Lbl("Dir",  "Direct : " + dirName,        x, y, dirClr        );    y += s;
    Lbl("L1",   "────────────────────────",   x, y, C'45,58,105'  );    y += s-2;
    Lbl("Bal",  StringFormat("Balance: $%.2f", balance),    x, y, clrSilver); y += s;
    Lbl("Ini",  StringFormat("Initial: $%.2f", InitBalance), x, y, clrSilver); y += s;
    Lbl("DayP", StringFormat("Day P/L: $%.2f", DayProfit),  x, y, cDayP);     y += s;
    Lbl("FP",   StringFormat("Float  : $%.2f  (%.2f%%)", totalProfit, pnlPct),
                x, y, cProfit);                                                y += s;
    Lbl("L2",   "────────────────────────",   x, y, C'45,58,105'  );    y += s-2;
    Lbl("DD",   StringFormat("DD Now : %.2f%%", ddPct),     x, y,
                ddPct > 15 ? clrOrangeRed : clrSilver);                       y += s;
    Lbl("MDD",  StringFormat("DD Max : %.2f%%", MaxDrawdownPct), x, y,
                MaxDrawdownPct > 25 ? clrTomato : clrSilver);                 y += s;
    Lbl("Sprd", StringFormat("Spread : %.0f pts", spread),  x, y, clrSilver); y += s;
    Lbl("L3",   "────────────────────────",   x, y, C'45,58,105'  );    y += s-2;
    Lbl("BuyP", StringFormat("Buy P/L: $%.2f", buyProfit),  x, y, cBuyP);     y += s;
    Lbl("BuyC", StringFormat("Buy Ord: %d   Lot: %.2f", nBuy,  lotBuy),  x, y, clrSilver); y += s;
    Lbl("SelP", StringFormat("Sel P/L: $%.2f", sellProfit), x, y, cSellP);    y += s;
    Lbl("SelC", StringFormat("Sel Ord: %d   Lot: %.2f", nSell, lotSell), x, y, clrSilver); y += s;
    Lbl("Tot",  StringFormat("Total  : %d orders", nBuy + nSell), x, y, clrSilver);        y += s;

    // ── NỘI DUNG PANEL 2 (Nút điều khiển) ──
    y = 388;
    Lbl("P2T", "═══  ĐIỀU KHIỂN LỆNH  ═══", x, y, C'90,140,230', 9); y += s + 2;

    int bh = 22;
    CreateBtn("BtnCloseAll",    "  Close All",     12,  y, 234, bh, C'20,60,150',  C'80,130,230'); y += bh + 4;
    CreateBtn("BtnCloseBuy",    "▲ Close Buy",     12,  y, 114, bh, C'0,105,45',   C'45,185,90' );
    CreateBtn("BtnCloseProfit", "$ Close Profit",  130, y, 114, bh, C'0,110,100',  C'40,190,170'); y += bh + 4;
    CreateBtn("BtnCloseSell",   "▼ Close Sell",    12,  y, 114, bh, C'145,15,15',  C'230,65,65' );
    CreateBtn("BtnCloseLoss",   "✕ Close Loss",    130, y, 114, bh, C'140,35,20',  C'210,80,55' );

    // ── NỘI DUNG PANEL 3 (Thống kê) ──
    y = 528;
    Lbl("P3T", "═══  THỐNG KÊ  ═══", x, y, C'90,140,230', 9); y += s + 2;
    // y=546: table starts here

    color sepClr = C'45,65,120';
    int tblTop = y, tblH = 5*(s-2); // (header + 4 data rows) × 14px = 70px

    // Separator lines created BEFORE text so text renders on top (Z-order)
    // Vertical column dividers
    CreateRect("P3VC1", 66,  tblTop, 1, tblH, sepClr);
    CreateRect("P3VC2", 116, tblTop, 1, tblH, sepClr);
    CreateRect("P3VC3", 168, tblTop, 1, tblH, sepClr);
    CreateRect("P3VC4", 216, tblTop, 1, tblH, sepClr);
    // Horizontal dividers: after header + between data rows
    for(int si = 0; si < 4; si++)
        CreateRect("P3HR"+IntegerToString(si), 7, tblTop + (si+1)*(s-2) - 1, 248, 1, sepClr);

    int cx0=12, cx1=68, cx2=118, cx3=170, cx4=218;
    Lbl("TH0", "Date ",  cx0, y, C'100,125,195', 8);
    Lbl("TH1", "Pips ",  cx1, y, C'100,125,195', 8);
    Lbl("TH2", "Profit", cx2, y, C'100,125,195', 8);
    Lbl("TH3", "Gain ",  cx3, y, C'100,125,195', 8);
    Lbl("TH4", "Lot  ",  cx4, y, C'100,125,195', 8);
    y += s - 2;

    TimeToStruct(TimeCurrent(), dt);
    datetime todayStart = StringToTime(StringFormat("%04d.%02d.%02d 00:00:00", dt.year, dt.mon, dt.day));
    int dow = dt.day_of_week; if(dow == 0) dow = 7;
    datetime weekStart  = todayStart - (dow - 1) * 86400;
    datetime monthStart = StringToTime(StringFormat("%04d.%02d.01 00:00:00", dt.year, dt.mon));
    datetime yearStart  = StringToTime(StringFormat("%04d.01.01 00:00:00", dt.year));
    datetime nowT       = TimeCurrent();

    PeriodStats allStats[4];
    allStats[0] = GetPeriodStats(todayStart, nowT);
    allStats[1] = GetPeriodStats(weekStart,  nowT);
    allStats[2] = GetPeriodStats(monthStart, nowT);
    allStats[3] = GetPeriodStats(yearStart,  nowT);

    string rowKeys[4];
    rowKeys[0]="Today"; rowKeys[1]="Week"; rowKeys[2]="Month"; rowKeys[3]="Year";

    for(int r = 0; r < 4; r++) {
        color rc = (allStats[r].profit >= 0) ? clrLimeGreen : clrTomato;
        string ri = IntegerToString(r);
        Lbl("TR"+ri+"L", rowKeys[r],                                cx0, y, clrSilver, 8);
        Lbl("TR"+ri+"P", StringFormat("%.0f",   allStats[r].pips),  cx1, y, rc, 8);
        Lbl("TR"+ri+"$", StringFormat("$%.1f",  allStats[r].profit), cx2, y, rc, 8);
        Lbl("TR"+ri+"G", StringFormat("%.1f%%", allStats[r].gain),  cx3, y, rc, 8);
        Lbl("TR"+ri+"V", StringFormat("%.2f",   allStats[r].lot),   cx4, y, clrSilver, 8);
        y += s - 2;
    }

    // ── PANEL 4: VÀO LỆNH THỦ CÔNG (chỉ hiện khi Bán Tự Động) ──
    if(InpBotMode == MODE_SEMI_AUTO) {
        string bg4 = GUI + "BG4";
        if(ObjectFind(0, bg4) < 0) {
            ObjectCreate(0, bg4, OBJ_RECTANGLE_LABEL, 0, 0, 0);
            ObjectSetInteger(0, bg4, OBJPROP_CORNER,      CORNER_LEFT_UPPER);
            ObjectSetInteger(0, bg4, OBJPROP_XDISTANCE,   5);
            ObjectSetInteger(0, bg4, OBJPROP_YDISTANCE,   675);
            ObjectSetInteger(0, bg4, OBJPROP_XSIZE,       252);
            ObjectSetInteger(0, bg4, OBJPROP_YSIZE,       58);
            ObjectSetInteger(0, bg4, OBJPROP_BGCOLOR,     C'20,14,14');
            ObjectSetInteger(0, bg4, OBJPROP_BORDER_TYPE, BORDER_FLAT);
            ObjectSetInteger(0, bg4, OBJPROP_COLOR,       C'160,60,60');
            ObjectSetInteger(0, bg4, OBJPROP_WIDTH,       1);
            ObjectSetInteger(0, bg4, OBJPROP_BACK,        false);
            ObjectSetInteger(0, bg4, OBJPROP_SELECTABLE,  false);
        }
        int y4 = 685;
        Lbl("P4T", "═══  VÀO LỆNH THỦ CÔNG  ═══", x, y4, C'230,100,100', 9); y4 += s + 2;
        CreateBtn("BtnOpenBuy",  "▲ Open Buy",  12,  y4, 114, bh, C'0,80,20',  C'30,200,80');
        CreateBtn("BtnOpenSell", "▼ Open Sell", 130, y4, 114, bh, C'100,0,0',  C'220,40,40');
    } else {
        ObjectDelete(0, GUI + "BG4");
        ObjectDelete(0, GUI + "P4T");
        ObjectDelete(0, GUI + "BtnOpenBuy");
        ObjectDelete(0, GUI + "BtnOpenSell");
    }

    ChartRedraw(0);
}

void RemoveGUI() { ObjectsDeleteAll(0, GUI); }

//+------------------------------------------------------------------+
//| INIT DCA ARRAYS                                                  |
//+------------------------------------------------------------------+
void InitDCA() {
    DCA_Mode[0]=InpDCAMode; DCA_Mult[0]=InpDCA1Mult; DCA_MaxOrd[0]=InpDCA1Max;
    DCA_Dist[0]=InpDCA1Dist; DCA_TP[0]=InpDCA1TP;    DCA_SL[0]=InpDCA1SL;

    DCA_Mode[1]=InpDCAMode; DCA_Mult[1]=InpDCA2Mult; DCA_MaxOrd[1]=InpDCA2Max;
    DCA_Dist[1]=InpDCA2Dist; DCA_TP[1]=InpDCA2TP;    DCA_SL[1]=InpDCA2SL;

    DCA_Mode[2]=InpDCAMode; DCA_Mult[2]=InpDCA3Mult; DCA_MaxOrd[2]=InpDCA3Max;
    DCA_Dist[2]=InpDCA3Dist; DCA_TP[2]=InpDCA3TP;    DCA_SL[2]=InpDCA3SL;

    DCA_Mode[3]=InpDCAMode; DCA_Mult[3]=InpDCA4Mult; DCA_MaxOrd[3]=InpDCA4Max;
    DCA_Dist[3]=InpDCA4Dist; DCA_TP[3]=InpDCA4TP;    DCA_SL[3]=InpDCA4SL;

    DCA_Mode[4]=InpDCAMode; DCA_Mult[4]=InpDCA5Mult; DCA_MaxOrd[4]=InpDCA5Max;
    DCA_Dist[4]=InpDCA5Dist; DCA_TP[4]=InpDCA5TP;    DCA_SL[4]=InpDCA5SL;

    DCA_Mode[5]=InpDCAMode; DCA_Mult[5]=InpDCA6Mult; DCA_MaxOrd[5]=InpDCA6Max;
    DCA_Dist[5]=InpDCA6Dist; DCA_TP[5]=InpDCA6TP;    DCA_SL[5]=InpDCA6SL;

    DCA_Mode[6]=InpDCAMode; DCA_Mult[6]=InpDCA7Mult; DCA_MaxOrd[6]=InpDCA7Max;
    DCA_Dist[6]=InpDCA7Dist; DCA_TP[6]=InpDCA7TP;    DCA_SL[6]=InpDCA7SL;

    DCA_Mode[7]=InpDCAMode; DCA_Mult[7]=InpDCA8Mult; DCA_MaxOrd[7]=InpDCA8Max;
    DCA_Dist[7]=InpDCA8Dist; DCA_TP[7]=InpDCA8TP;    DCA_SL[7]=InpDCA8SL;
}

void InitPyra() {
    PYRA_Mode[0]=InpPyraMode; PYRA_Mult[0]=InpPyra1Mult; PYRA_MaxOrd[0]=InpPyra1Max;
    PYRA_Dist[0]=InpPyra1Dist; PYRA_TP[0]=InpPyra1TP;   PYRA_SL[0]=InpPyra1SL;

    PYRA_Mode[1]=InpPyraMode; PYRA_Mult[1]=InpPyra2Mult; PYRA_MaxOrd[1]=InpPyra2Max;
    PYRA_Dist[1]=InpPyra2Dist; PYRA_TP[1]=InpPyra2TP;   PYRA_SL[1]=InpPyra2SL;

    PYRA_Mode[2]=InpPyraMode; PYRA_Mult[2]=InpPyra3Mult; PYRA_MaxOrd[2]=InpPyra3Max;
    PYRA_Dist[2]=InpPyra3Dist; PYRA_TP[2]=InpPyra3TP;   PYRA_SL[2]=InpPyra3SL;

    PYRA_Mode[3]=InpPyraMode; PYRA_Mult[3]=InpPyra4Mult; PYRA_MaxOrd[3]=InpPyra4Max;
    PYRA_Dist[3]=InpPyra4Dist; PYRA_TP[3]=InpPyra4TP;   PYRA_SL[3]=InpPyra4SL;

    PYRA_Mode[4]=InpPyraMode; PYRA_Mult[4]=InpPyra5Mult; PYRA_MaxOrd[4]=InpPyra5Max;
    PYRA_Dist[4]=InpPyra5Dist; PYRA_TP[4]=InpPyra5TP;   PYRA_SL[4]=InpPyra5SL;

    PYRA_Mode[5]=InpPyraMode; PYRA_Mult[5]=InpPyra6Mult; PYRA_MaxOrd[5]=InpPyra6Max;
    PYRA_Dist[5]=InpPyra6Dist; PYRA_TP[5]=InpPyra6TP;   PYRA_SL[5]=InpPyra6SL;

    PYRA_Mode[6]=InpPyraMode; PYRA_Mult[6]=InpPyra7Mult; PYRA_MaxOrd[6]=InpPyra7Max;
    PYRA_Dist[6]=InpPyra7Dist; PYRA_TP[6]=InpPyra7TP;   PYRA_SL[6]=InpPyra7SL;

    PYRA_Mode[7]=InpPyraMode; PYRA_Mult[7]=InpPyra8Mult; PYRA_MaxOrd[7]=InpPyra8Max;
    PYRA_Dist[7]=InpPyra8Dist; PYRA_TP[7]=InpPyra8TP;   PYRA_SL[7]=InpPyra8SL;
}

//+------------------------------------------------------------------+
//| EVENT HANDLERS                                                   |
//+------------------------------------------------------------------+
int OnInit() {
    Trade.SetExpertMagicNumber(InpMagic);
    Trade.SetDeviationInPoints(50);
    Trade.SetTypeFilling(ORDER_FILLING_RETURN);

    InitDCA();
    InitPyra();

    // Create indicator handles
    hEMAFast = iMA(_Symbol, InpSignalTF, InpEMAFast, 0, MODE_EMA, PRICE_CLOSE);
    hEMASlow = iMA(_Symbol, InpSignalTF, InpEMASlow, 0, MODE_EMA, PRICE_CLOSE);
    hBB      = iBands(_Symbol, InpSignalTF, InpBBPeriod, 0, InpBBDev, PRICE_CLOSE);
    hIchi    = iIchimoku(_Symbol, InpSignalTF, InpIchiTenkan, InpIchiKijun, InpIchiSenkou);

    if(hEMAFast == INVALID_HANDLE || hEMASlow == INVALID_HANDLE ||
       hBB == INVALID_HANDLE      || hIchi    == INVALID_HANDLE) {
        Print("RTB: ERROR — failed to create indicator handles!");
        return INIT_FAILED;
    }

    InitBalance    = AccountInfoDouble(ACCOUNT_BALANCE);
    MaxDrawdownPct = 0;
    TrailBuy       = 0;
    TrailSell      = 0;
    LastEntryTime  = 0;
    LastDay        = -1;

    EventSetTimer(1);
    Print("RTB: Initialized. Magic=", InpMagic, " Signal=", EnumToString(InpSignalMode));
    return INIT_SUCCEEDED;
}

void OnDeinit(const int reason) {
    EventKillTimer();
    RemoveGUI();
    IndicatorRelease(hEMAFast);
    IndicatorRelease(hEMASlow);
    IndicatorRelease(hBB);
    IndicatorRelease(hIchi);
}

void OnTick() {
    // Entry signals only fire when no position exists for that direction
    CheckEntry();
    // Stealth TP/SL check runs on every tick for precision
    if(InpStealthMode) CheckExit();
}

void OnTimer() {
    UpdateDayProfit();

    if(CountBuy()  == 0) TrailBuy  = 0;
    if(CountSell() == 0) TrailSell = 0;

    // Exit checks (basket close conditions)
    if(!InpStealthMode) CheckExit();

    // Trimming
    CheckTrimming();

    // Trailing stop
    CheckTrailing();

    // DCA scale-in
    if(CountBuy()  > 0) CheckDCA(POSITION_TYPE_BUY);
    if(CountSell() > 0) CheckDCA(POSITION_TYPE_SELL);

    // Pyramiding (add to winners)
    if(CountBuy()  > 0) CheckPyramiding(POSITION_TYPE_BUY);
    if(CountSell() > 0) CheckPyramiding(POSITION_TYPE_SELL);

    // GUI refresh every second
    UpdateGUI();
}

void OnTradeTransaction(const MqlTradeTransaction& trans,
                        const MqlTradeRequest&     req,
                        const MqlTradeResult&      res) {
    // Cập nhật Day P/L ngay khi có deal đóng, không chờ timer 1 giây
    if(trans.type == TRADE_TRANSACTION_DEAL_ADD) {
        UpdateDayProfit();
        UpdateGUI();
    }
}

void OnChartEvent(const int id, const long& lparam, const double& dparam, const string& sparam) {
    if(id != CHARTEVENT_OBJECT_CLICK) return;
    if     (sparam == GUI + "BtnCloseAll")    CloseAll();
    else if(sparam == GUI + "BtnCloseBuy")    CloseAll(POSITION_TYPE_BUY);
    else if(sparam == GUI + "BtnCloseSell")   CloseAll(POSITION_TYPE_SELL);
    else if(sparam == GUI + "BtnCloseProfit") CloseAllProfit();
    else if(sparam == GUI + "BtnCloseLoss")   CloseAllLoss();
    else if(sparam == GUI + "BtnOpenBuy")     OpenOrder(ORDER_TYPE_BUY,  InpLotSize, InpTP_Points, InpSL_Points);
    else if(sparam == GUI + "BtnOpenSell")    OpenOrder(ORDER_TYPE_SELL, InpLotSize, InpTP_Points, InpSL_Points);
    else return;
    ObjectSetInteger(0, sparam, OBJPROP_STATE, false);
    ChartRedraw(0);
}