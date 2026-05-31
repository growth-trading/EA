//+------------------------------------------------------------------+
//|                                            BotDuyDuong.mq5       |
//|               BB Breakout — Bollinger Bands + Stochastic Filter  |
//|               Author  : Duy Duong                                |
//|               Version : 2.0  |  Date: 30.05.2026                 |
//+------------------------------------------------------------------+
#property copyright "Duy Duong"
#property version   "2.00"
#property strict

#include <Trade\Trade.mqh>

//+------------------------------------------------------------------+
//|  INPUTS                                                          |
//+------------------------------------------------------------------+

input group "══════ BOLLINGER BANDS ══════"
input int    PeriodBB   = 20;   // BB Period
input double Deviation  = 2.0;  // Standard Deviation

input group "══════ SUPER TREND ══════"
input bool   UseSTFilter  = true;   // Bật/tắt bộ lọc Supertrend
input int    STPeriod     = 10;     // ATR Period
input double STMult       = 1.2;    // ATR Multiplier (Factor)

input group "══════ STOCHASTIC FILTER ══════"
input bool   UseStochFilter  = true;  // Bật/tắt bộ lọc Stochastic
input int    StochKPeriod    = 14;    // Stochastic %K Length
input int    StochDPeriod    = 3;     // Stochastic %D Smoothing
input int    StochSlowing    = 1;     // Stochastic %K Smoothing
input double StochOverbought = 70.0;  // Ngưỡng quá mua — SELL phải chạm đây
input double StochOversold   = 30.0;  // Ngưỡng quá bán — BUY phải chạm đây
input double StochMidLevel   = 50.0;  // Ngưỡng giữa — không được cắt qua

input group "══════ QUẢN LÝ VỐN ══════"
input double RiskPercent = 1.0;  // Rủi ro mỗi lệnh (%)
input double TakeProfit1 = 1.0;  // TP1: đóng 50% tại đây (RR), 0 = tắt
input double TakeProfit  = 2.0;  // TP2: đóng 50% còn lại (RR)

input group "══════ PHIÊN GIAO DỊCH ══════"
input string StartTime  = "00:00";  // Phiên 1: Bắt đầu (00:00-00:00 = tắt)
input string EndTime    = "23:00";  // Phiên 1: Kết thúc
input string StartTime2 = "00:00";  // Phiên 2: Bắt đầu (00:00-00:00 = tắt)
input string EndTime2   = "23:00";  // Phiên 2: Kết thúc
input int    MaxTradesInWindow = 2;  // Số lệnh tối đa mỗi phiên

input group "══════ TIN TỨC (AUTO MT5 CALENDAR) ══════"
input bool   InpNewsPauseEnabled = true;              // Bật tự động dừng khi có tin
input int    InpNewsPauseBefore  = 30;                // Dừng trước tin (phút)
input int    InpNewsResumeAfter  = 30;                // Mở lại sau tin (phút)
input string InpNewsCurrencies   = "USD,EUR,GBP,JPY,XAU"; // Tiền tệ cần lọc (cách nhau bởi dấu ,)
input int    InpNewsScanHours    = 24;                // Quét lịch tin trước X giờ
input int    InpNewsUpdateSec    = 60;                // Cập nhật lịch mỗi X giây

input group "══════ PANEL HIỂN THỊ ══════"
input int  InpPanelX = 5;    // Vị trí X (pixel từ trái)
input int  InpPanelY = 18;   // Vị trí Y (pixel từ trên)
input int  InpPanelW = 258;  // Chiều rộng panel

input group "══════ GENERAL ══════"
ulong MagicNum = 20260513;   // Magic Number

//+------------------------------------------------------------------+
//|  GLOBALS                                                         |
//+------------------------------------------------------------------+

CTrade trade;
int    handleBB;
int    handleStoch;
int    handleATR;

datetime buyTriggerTime       = 0;
datetime sellTriggerTime      = 0;
double   buyTriggerCandleHigh = 0.0;  // high nến trigger BUY (nến phá Lower BB)
double   sellTriggerCandleLow = 0.0;  // low nến trigger SELL (nến phá Upper BB)
bool     buyTouchedMiddle      = false; // giá hồi đã chạm đường giữa trong setup BUY
bool     sellTouchedMiddle     = false; // giá hồi đã chạm đường giữa trong setup SELL
double   buyBullishPhaseLow    = 0.0;  // đáy thấp nhất của các nến tăng trong phase bounce (BUY)
double   sellBearishPhaseHigh  = 0.0;  // đỉnh cao nhất của các nến giảm trong phase pullback (SELL)
datetime buyPeakMarkTime       = 0;    // thời điểm vẽ mark đỉnh BUY (để xóa)
datetime sellBottomMarkTime    = 0;    // thời điểm vẽ mark đáy SELL (để xóa)

// Setup state: 0=idle 1=setup đang theo dõi 2=lệnh đã đặt
int    checkBuy  = 0;
int    checkSell = 0;
int    checkFirstbuy  = 0;
int    checkFirstsell = 0;

// BUY tracking
double buyMinLow       = 0.0;  // Đáy thấp nhất kể từ trigger
double buyPeakHigh     = 0.0;  // Đỉnh của nến tăng xác nhận
double buyHighestPrice = 0.0;  // Đỉnh cao nhất tổng thể
double buyLowestSinceLimit = 0.0;

// SELL tracking
double sellMaxHigh    = 0.0;   // Đỉnh cao nhất kể từ trigger
double sellBottomLow  = 0.0;   // Đáy của nến giảm xác nhận
double sellLowestPrice = 0.0;  // Đáy thấp nhất tổng thể
double sellHighestSinceLimit = 0.0;

// Session tracking
int  lastResetDay    = -1;

// TP1 partial close tracking
#define MAX_TP1_TRACK 10
ulong g_tp1Tickets[MAX_TP1_TRACK];
int   g_tp1Count = 0;

// News auto cache
struct NewsItem { datetime time; string currency; };
NewsItem g_newsCache[];
datetime g_lastNewsUpdate = 0;

// GUI
datetime lastBarTime = 0;
const string GUI = "BDD_";

//+------------------------------------------------------------------+
//|  TIME HELPERS                                                    |
//+------------------------------------------------------------------+

int TimeStringToMinutes(const string timeStr)
{
   string parts[];
   if(StringSplit(timeStr, ':', parts) != 2) return -1;
   int hour   = (int)StringToInteger(parts[0]);
   int minute = (int)StringToInteger(parts[1]);
   if(hour < 0 || hour > 23 || minute < 0 || minute > 59) return -1;
   return hour * 60 + minute;
}

bool IsTimeInRange(int cur, int start, int end)
{
   if(start == -1 || end == -1)       return false;
   if(start == 0  && end == 0)        return false;
   if(start == end)                   return true;
   if(start < end) return (cur >= start && cur <= end);
   return (cur >= start || cur <= end);
}

bool IsInSession1()
{
   MqlDateTime tm; TimeToStruct(TimeCurrent(), tm);
   int cur   = tm.hour * 60 + tm.min;
   int start = TimeStringToMinutes(StartTime);
   int end   = TimeStringToMinutes(EndTime);
   return IsTimeInRange(cur, start, end);
}

bool IsInSession2()
{
   MqlDateTime tm; TimeToStruct(TimeCurrent(), tm);
   int cur   = tm.hour * 60 + tm.min;
   int start = TimeStringToMinutes(StartTime2);
   int end   = TimeStringToMinutes(EndTime2);
   return IsTimeInRange(cur, start, end);
}

bool IsTradingTimeVietnam() { return (IsInSession1() || IsInSession2()); }

int GetCurrentSession()
{
   if(IsInSession1()) return 1;
   if(IsInSession2()) return 2;
   return 0;
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
   int n = StringSplit(InpNewsCurrencies, StringGetCharacter(",", 0), currencies);
   for(int i = 0; i < n; i++) { StringTrimLeft(currencies[i]); StringTrimRight(currencies[i]); }

   for(int i = 0; i < total; i++)
   {
      MqlCalendarEvent  event;
      MqlCalendarCountry country;
      if(!CalendarEventById(values[i].event_id, event))   continue;
      if(event.importance != CALENDAR_IMPORTANCE_HIGH)    continue;
      if(!CalendarCountryById(event.country_id, country)) continue;

      bool found = false;
      for(int j = 0; j < n; j++)
         if(currencies[j] == country.currency) { found = true; break; }
      if(!found) continue;

      int idx = ArraySize(g_newsCache);
      ArrayResize(g_newsCache, idx + 1);
      g_newsCache[idx].time     = values[i].time;
      g_newsCache[idx].currency = country.currency;
   }
}

bool IsInNewsZone()
{
   datetime now    = TimeCurrent();
   datetime before = (datetime)(InpNewsPauseBefore * 60);
   datetime after  = (datetime)(InpNewsResumeAfter  * 60);
   for(int i = 0; i < ArraySize(g_newsCache); i++)
   {
      datetime t = g_newsCache[i].time;
      if(now >= t - before && now <= t + after) return true;
   }
   return false;
}

//+------------------------------------------------------------------+
//|  ORDER HELPERS                                                   |
//+------------------------------------------------------------------+

bool HasPosition(string symbol)    { return PositionSelect(symbol); }

bool HasPendingOrder(string symbol)
{
   for(int i = OrdersTotal() - 1; i >= 0; i--)
      if(OrderGetTicket(i) > 0 && OrderGetString(ORDER_SYMBOL) == symbol) return true;
   return false;
}

bool HasAnyOrder(string symbol) { return HasPosition(symbol) || HasPendingOrder(symbol); }

void CloseAllPendingOrders()
{
   for(int i = OrdersTotal() - 1; i >= 0; i--)
   {
      ulong tk = OrderGetTicket(i);
      if(tk > 0 && OrderGetString(ORDER_SYMBOL) == _Symbol)
         trade.OrderDelete(tk);
   }
}

double GetSpread() { return SymbolInfoDouble(_Symbol, SYMBOL_ASK) - SymbolInfoDouble(_Symbol, SYMBOL_BID); }

//+------------------------------------------------------------------+
//|  TP1 PARTIAL CLOSE                                               |
//+------------------------------------------------------------------+

bool IsTP1Done(ulong ticket)
{
   for(int i = 0; i < g_tp1Count; i++)
      if(g_tp1Tickets[i] == ticket) return true;
   return false;
}

void MarkTP1Done(ulong ticket)
{
   if(g_tp1Count < MAX_TP1_TRACK)
      g_tp1Tickets[g_tp1Count++] = ticket;
}

void ResetTP1Track()
{
   g_tp1Count = 0;
   ArrayInitialize(g_tp1Tickets, 0);
}

void CheckPartialClose()
{
   if(TakeProfit1 <= 0) return;

   for(int i = PositionsTotal() - 1; i >= 0; i--)
   {
      ulong ticket = PositionGetTicket(i);
      if(ticket == 0) continue;
      if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
      if(PositionGetInteger(POSITION_MAGIC) != (long)MagicNum) continue;
      if(IsTP1Done(ticket)) continue;

      double sl   = PositionGetDouble(POSITION_SL);
      if(sl <= 0) continue;  // chờ SL được set

      double open = PositionGetDouble(POSITION_PRICE_OPEN);
      double vol  = PositionGetDouble(POSITION_VOLUME);
      double curTP = PositionGetDouble(POSITION_TP);
      long   type = PositionGetInteger(POSITION_TYPE);
      double risk = MathAbs(open - sl);
      if(risk <= 0) continue;

      double tp1Price = (type == POSITION_TYPE_BUY)
                        ? open + risk * TakeProfit1
                        : open - risk * TakeProfit1;

      double bid = SymbolInfoDouble(_Symbol, SYMBOL_BID);
      double ask = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
      bool   hit = (type == POSITION_TYPE_BUY  && bid >= tp1Price) ||
                   (type == POSITION_TYPE_SELL && ask <= tp1Price);
      if(!hit) continue;

      // Đóng 50% lot
      double lotStep = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
      double minLot  = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
      double halfLot = MathFloor((vol / 2.0) / lotStep) * lotStep;
      if(halfLot < minLot) halfLot = minLot;
      if(halfLot >= vol)   halfLot = vol;  // không đóng quá toàn bộ

      if(trade.PositionClosePartial(ticket, halfLot))
      {
         MarkTP1Done(ticket);
         Print("TP1 đóng 50% lot=", halfLot, " ticket=", ticket);
      }
   }
}

double CalculateLotBySize(double entryPrice, double slPrice)
{
   double dist = MathAbs(entryPrice - slPrice);
   if(dist <= 0) return SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
   double balance   = AccountInfoDouble(ACCOUNT_BALANCE);
   double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
   double tickSize  = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
   double lotStep   = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
   double minLot    = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
   double maxLot    = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
   double risk      = balance * RiskPercent / 100.0;
   double perLot    = (dist / tickSize) * tickValue;
   if(perLot <= 0) return minLot;
   double lot = MathFloor(risk / perLot / lotStep) * lotStep;
   return NormalizeDouble(MathMax(minLot, MathMin(maxLot, lot)), 2);
}

//+------------------------------------------------------------------+
//|  SUPERTREND (ATR-based, tự tính)                                 |
//+------------------------------------------------------------------+

// Trả về upper band và lower band của Supertrend tại bar[shift]
bool GetSTBands(int shift, double &upper, double &lower)
{
   int total = STPeriod * 3 + shift + 2;
   double atr[], cls[], hgh[], lw[];
   ArraySetAsSeries(atr, true); ArraySetAsSeries(cls, true);
   ArraySetAsSeries(hgh, true); ArraySetAsSeries(lw,  true);
   if(CopyBuffer(handleATR, 0, 0, total, atr) < total) return false;
   if(CopyClose (_Symbol, _Period, 0, total, cls) < total) return false;
   if(CopyHigh  (_Symbol, _Period, 0, total, hgh) < total) return false;
   if(CopyLow   (_Symbol, _Period, 0, total, lw)  < total) return false;

   double pu = 0, pl = 0;
   bool   pb = true;
   for(int i = total - 1; i >= shift; i--)
   {
      double hl2 = (hgh[i] + lw[i]) / 2.0;
      double u   = hl2 + STMult * atr[i];
      double l   = hl2 - STMult * atr[i];
      if(i < total - 1)
      {
         l = (l > pl || cls[i+1] < pl) ? l : pl;
         u = (u < pu || cls[i+1] > pu) ? u : pu;
      }
      bool bull;
      if(i == total - 1)         bull = (cls[i] >= (hgh[i] + lw[i]) / 2.0);
      else if(pb  && cls[i] < l) bull = false;
      else if(!pb && cls[i] > u) bull = true;
      else                       bull = pb;
      pu = u; pl = l; pb = bull;
      if(i == shift) { upper = u; lower = l; return true; }
   }
   return false;
}

//+------------------------------------------------------------------+
//|  STOCHASTIC FILTER                                               |
//+------------------------------------------------------------------+

// barsBack: số nến cần kiểm tra (tính từ bar[1] trở về quá khứ)
bool CheckStochBuy(int barsBack)
{
   if(barsBack < 2) barsBack = 2;
   double stochD[];
   ArraySetAsSeries(stochD, true);
   if(CopyBuffer(handleStoch, 1, 1, barsBack, stochD) < barsBack) return true;  // buffer 1 = %D
   double minV = stochD[0], maxV = stochD[0];
   for(int i = 0; i < barsBack; i++) {
      if(stochD[i] < minV) minV = stochD[i];
      if(stochD[i] > maxV) maxV = stochD[i];
   }
   return (minV <= StochOversold && maxV < StochMidLevel);
}

bool CheckStochSell(int barsBack)
{
   if(barsBack < 2) barsBack = 2;
   double stochD[];
   ArraySetAsSeries(stochD, true);
   if(CopyBuffer(handleStoch, 1, 1, barsBack, stochD) < barsBack) return true;  // buffer 1 = %D
   double minV = stochD[0], maxV = stochD[0];
   for(int i = 0; i < barsBack; i++) {
      if(stochD[i] < minV) minV = stochD[i];
      if(stochD[i] > maxV) maxV = stochD[i];
   }
   return (maxV >= StochOverbought && minV > StochMidLevel);
}

//+------------------------------------------------------------------+
//|  SL/TP UPDATE                                                    |
//+------------------------------------------------------------------+

void UpdateStopLossFromHistory()
{
   for(int i = PositionsTotal() - 1; i >= 0; i--)
   {
      ulong ticket = PositionGetTicket(i);
      if(ticket == 0) continue;
      if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
      if(PositionGetInteger(POSITION_MAGIC) != (long)MagicNum) continue;
      if(PositionGetDouble(POSITION_SL) > 0.0) continue;

      double openPrice = PositionGetDouble(POSITION_PRICE_OPEN);
      long   type      = PositionGetInteger(POSITION_TYPE);
      double newSL = 0, newTP = 0;

      if(type == POSITION_TYPE_BUY && buyMinLow > 0.0)
      {
         newSL = NormalizeDouble(buyMinLow, _Digits);
         double risk = openPrice - newSL;
         if(risk <= 0) continue;
         newTP = NormalizeDouble(openPrice + risk * TakeProfit, _Digits);
      }
      else if(type == POSITION_TYPE_SELL && sellMaxHigh > 0.0)
      {
         newSL = NormalizeDouble(sellMaxHigh + GetSpread(), _Digits);
         double risk = newSL - openPrice;
         if(risk <= 0) continue;
         newTP = NormalizeDouble(openPrice - risk * TakeProfit, _Digits);
      }

      if(newSL > 0.0)
         trade.PositionModify(ticket, newSL, newTP);
   }
}

//+------------------------------------------------------------------+
//|  COUNT TRADES IN SESSION WINDOW                                  |
//+------------------------------------------------------------------+

int CountTradesInWindow(string tStart, string tEnd)
{
   int count = 0;
   datetime now = TimeCurrent();
   MqlDateTime tm; TimeToStruct(now, tm);
   int winStart = TimeStringToMinutes(tStart);
   int winEnd   = TimeStringToMinutes(tEnd);
   int curMin   = tm.hour * 60 + tm.min;
   if(!IsTimeInRange(curMin, winStart, winEnd)) return 0;
   tm.hour = 0; tm.min = 0; tm.sec = 0;
   datetime startOfDay = StructToTime(tm);
   if(!HistorySelect(startOfDay, now)) return 0;
   for(int i = 0; i < HistoryDealsTotal(); i++)
   {
      ulong tk = HistoryDealGetTicket(i);
      if(tk == 0) continue;
      if(HistoryDealGetString(tk, DEAL_SYMBOL) != _Symbol) continue;
      if(HistoryDealGetInteger(tk, DEAL_MAGIC) != MagicNum) continue;
      if(HistoryDealGetInteger(tk, DEAL_ENTRY) != DEAL_ENTRY_IN) continue;
      MqlDateTime dtm;
      TimeToStruct((datetime)HistoryDealGetInteger(tk, DEAL_TIME), dtm);
      int dealMin = dtm.hour * 60 + dtm.min;
      if(IsTimeInRange(dealMin, winStart, winEnd)) count++;
   }
   return count;
}

//+------------------------------------------------------------------+
//|  GUI HELPERS                                                     |
//+------------------------------------------------------------------+

void Lbl(string name, string text, int x, int y, color clr = clrSilver, int sz = 9)
{
   string obj = GUI + name;
   if(ObjectFind(0, obj) < 0)
   {
      ObjectCreate(0, obj, OBJ_LABEL, 0, 0, 0);
      ObjectSetInteger(0, obj, OBJPROP_CORNER,     CORNER_LEFT_UPPER);
      ObjectSetString(0,  obj, OBJPROP_FONT,       "Consolas");
      ObjectSetInteger(0, obj, OBJPROP_BACK,       false);
      ObjectSetInteger(0, obj, OBJPROP_SELECTABLE, false);
   }
   ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
   ObjectSetString(0,  obj, OBJPROP_TEXT,      text);
   ObjectSetInteger(0, obj, OBJPROP_FONTSIZE,  sz);
   ObjectSetInteger(0, obj, OBJPROP_COLOR,     clr);
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
   ObjectSetInteger(0, obj, OBJPROP_XDISTANCE, x); ObjectSetInteger(0, obj, OBJPROP_YDISTANCE, y);
   ObjectSetInteger(0, obj, OBJPROP_XSIZE,     w); ObjectSetInteger(0, obj, OBJPROP_YSIZE,     h);
   ObjectSetInteger(0, obj, OBJPROP_BGCOLOR,   bg); ObjectSetInteger(0, obj, OBJPROP_COLOR,    border);
}

void CreateBtn(string name, string text, int x, int y, int w, int h, color bg, color border = clrSilver)
{
   string obj = GUI + name;
   if(ObjectFind(0, obj) < 0)
   {
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

//+------------------------------------------------------------------+
//|  BAND MARK — Đánh dấu đỉnh/đáy lên chart                        |
//+------------------------------------------------------------------+

void DrawBandMark(string id, datetime t, double price, string txt, color clr, int sz = 9, int anchor = ANCHOR_CENTER)
{
   string name = GUI + "MK_" + id;
   if(ObjectFind(0, name) >= 0) return;
   ObjectCreate(0, name, OBJ_TEXT, 0, t, price);
   ObjectSetString(0,  name, OBJPROP_TEXT,      txt);
   ObjectSetString(0,  name, OBJPROP_FONT,      "Arial");
   ObjectSetInteger(0, name, OBJPROP_FONTSIZE,  sz);
   ObjectSetInteger(0, name, OBJPROP_COLOR,     clr);
   ObjectSetInteger(0, name, OBJPROP_ANCHOR,    anchor);
   ObjectSetInteger(0, name, OBJPROP_SELECTABLE, false);
   ObjectSetInteger(0, name, OBJPROP_BACK,      false);
}

void ClearBuyMarks()
{
   ObjectsDeleteAll(0, GUI + "MK_BT");
   ObjectsDeleteAll(0, GUI + "MK_BPK");
   buyTriggerTime  = 0;
   buyPeakMarkTime = 0;
   ChartRedraw(0);
}

void ClearSellMarks()
{
   ObjectsDeleteAll(0, GUI + "MK_ST");
   ObjectsDeleteAll(0, GUI + "MK_SBT");
   sellTriggerTime    = 0;
   sellBottomMarkTime = 0;
   ChartRedraw(0);
}

//+------------------------------------------------------------------+
//|  UPDATE GUI                                                      |
//+------------------------------------------------------------------+

void UpdateGUI()
{
   double balance   = AccountInfoDouble(ACCOUNT_BALANCE);
   double equity    = AccountInfoDouble(ACCOUNT_EQUITY);
   double spread    = (double)SymbolInfoInteger(_Symbol, SYMBOL_SPREAD);
   double floatPL   = equity - balance;

   // Tính P/L floating từ positions
   floatPL = 0;
   for(int i = PositionsTotal() - 1; i >= 0; i--)
   {
      if(!PositionGetTicket(i)) continue;
      if(PositionGetString(POSITION_SYMBOL) != _Symbol) continue;
      if(PositionGetInteger(POSITION_MAGIC) != (long)MagicNum) continue;
      floatPL += PositionGetDouble(POSITION_PROFIT) + PositionGetDouble(POSITION_SWAP);
   }

   MqlDateTime dt; TimeToStruct(TimeLocal(), dt);
   string tStr = StringFormat("%04d/%02d/%02d  %02d:%02d:%02d",
                              dt.year, dt.mon, dt.day, dt.hour, dt.min, dt.sec);

   // Trạng thái session
   bool   inS1      = IsInSession1();
   bool   inS2      = IsInSession2();
   bool   newsBlock = InpNewsPauseEnabled && IsInNewsZone();
   int    session   = GetCurrentSession();
   string sessStr;  color sessClr;
   if(newsBlock) { sessStr = "NEWS BLOCK"; sessClr = clrTomato; }
   else if(inS1) { sessStr = StringFormat("Phiên 1 [%s-%s]", StartTime, EndTime); sessClr = clrLimeGreen; }
   else if(inS2) { sessStr = StringFormat("Phiên 2 [%s-%s]", StartTime2, EndTime2); sessClr = clrLimeGreen; }
   else          { sessStr = "Ngoài phiên"; sessClr = clrSilver; }

   // Stochastic hiện tại
   double stochD[];
   ArraySetAsSeries(stochD, true);
   double stochVal = 0;
   if(CopyBuffer(handleStoch, 1, 1, 1, stochD) == 1) stochVal = stochD[0];  // buffer 1 = %D
   color stochClr = (stochVal >= StochOverbought) ? clrTomato
                  : (stochVal <= StochOversold)   ? clrLimeGreen : clrSilver;

   // BUY state
   string buyStateStr; color buyStateClr;
   switch(checkBuy) {
      case 0: buyStateStr = "IDLE";         buyStateClr = C'60,60,60';    break;
      case 1: buyStateStr = (checkFirstbuy == 0) ? "Chờ nến tăng" : "Chờ xác nhận đỉnh";
              buyStateClr = clrYellow; break;
      case 2: buyStateStr = "Lệnh đã đặt"; buyStateClr = clrLimeGreen;   break;
      default:buyStateStr = "?";            buyStateClr = clrSilver;
   }

   // SELL state
   string sellStateStr; color sellStateClr;
   switch(checkSell) {
      case 0: sellStateStr = "IDLE";          sellStateClr = C'60,60,60';   break;
      case 1: sellStateStr = (checkFirstsell == 0) ? "Chờ nến giảm" : "Chờ xác nhận đáy";
              sellStateClr = clrYellow; break;
      case 2: sellStateStr = "Lệnh đã đặt";  sellStateClr = clrLimeGreen;  break;
      default:sellStateStr = "?";             sellStateClr = clrSilver;
   }

   color cFloat = (floatPL >= 0) ? clrLimeGreen : clrTomato;

   int pX = InpPanelX, pY = InpPanelY, pW = InpPanelW;
   int s  = 16, sz = 9, cx = pX + 8;

   // ── PANEL 1: TRẠNG THÁI ──
   int p1H = 10 + (s+2) + (s-3) + 2*s + 8;
   CreateBG("BG1", pX, pY, pW, p1H, C'14,17,26', C'50,65,120');
   int y = pY + 8;
   Lbl("T0",  "  BB BREAKOUT  v2.0",               cx, y, C'80,160,255', sz+1); y += s+2;
   Lbl("S0",  "────────────────────────",           cx, y, C'45,58,105'        ); y += s-3;
   Lbl("Ti",  "Time    : " + tStr,                 cx, y, clrSilver,     sz   ); y += s;
   Lbl("Ss",  "Session : " + sessStr,              cx, y, sessClr,       sz   ); y += s;

   // ── PANEL 2: TÀI KHOẢN ──
   int p2Y = pY + p1H + 4;
   int p2H = 10 + (s+2) + (s-3) + 4*s + 8;
   CreateBG("BG2", pX, p2Y, pW, p2H, C'17,21,32', C'65,90,160');
   y = p2Y + 8;
   Lbl("A0",  "═══  TÀI KHOẢN  ═══",               cx, y, C'90,140,230', sz  ); y += s+2;
   Lbl("AS",  "────────────────────────",           cx, y, C'45,58,105'        ); y += s-3;
   Lbl("Bl",  StringFormat("Balance : $%.2f", balance),   cx, y, clrSilver, sz); y += s;
   Lbl("Eq",  StringFormat("Equity  : $%.2f", equity),    cx, y, clrSilver, sz); y += s;
   Lbl("FL",  StringFormat("Float   : $%.2f", floatPL),   cx, y, cFloat,    sz); y += s;
   Lbl("Sp",  StringFormat("Spread  : %.0f pts", spread), cx, y, clrSilver, sz); y += s;

   // ── PANEL 3: SETUP (BUY / SELL / CHỜ) ──
   bool hasBuy  = (checkBuy  > 0);
   bool hasSell = (checkSell > 0);

   color p3Bg, p3Bd, p3TitleClr, p3SepClr;
   string p3Title;
   if(hasBuy) {
      p3Bg = C'14,19,28'; p3Bd = C'0,100,50';
      p3TitleClr = C'40,200,90'; p3SepClr = C'0,80,30';
      p3Title = "═══  SETUP BUY  ═══";
   } else if(hasSell) {
      p3Bg = C'22,14,14'; p3Bd = C'120,0,0';
      p3TitleClr = C'230,65,65'; p3SepClr = C'80,0,0';
      p3Title = "═══  SETUP SELL  ═══";
   } else {
      p3Bg = C'17,21,32'; p3Bd = C'50,65,100';
      p3TitleClr = C'80,80,100'; p3SepClr = C'40,48,70';
      p3Title = "═══  SETUP  ═══";
   }

   int p3Y = p2Y + p2H + 4;
   int p3H = 10 + (s+2) + (s-3) + 5*s + 8;
   CreateBG("BG3", pX, p3Y, pW, p3H, p3Bg, p3Bd);
   y = p3Y + 8;
   Lbl("P3T", p3Title, cx, y, p3TitleClr, sz); y += s+2;
   Lbl("P3S", "────────────────────────", cx, y, p3SepClr); y += s-3;

   if(hasBuy)
   {
      Lbl("P3St", "State   : " + buyStateStr,  cx, y, buyStateClr, sz); y += s;
      Lbl("P3Lw", StringFormat("Đáy SL  : %s", buyMinLow > 0     ? DoubleToString(buyMinLow, _Digits) : "--"),   cx, y, clrSilver, sz); y += s;
      Lbl("P3Pk", StringFormat("Peak    : %s", buyPeakHigh > 0   ? DoubleToString(buyPeakHigh, _Digits) : "--"), cx, y, clrSilver, sz); y += s;
      Lbl("P3En", StringFormat("Entry   : %s", buyHighestPrice > 0 ? DoubleToString(buyHighestPrice + GetSpread(), _Digits) : "--"), cx, y, clrYellow, sz); y += s;
      Lbl("P3Tp", StringFormat("TP1/TP2 : RR×%.1f / RR×%.1f", TakeProfit1, TakeProfit), cx, y, C'100,200,120', sz); y += s;
   }
   else if(hasSell)
   {
      Lbl("P3St", "State   : " + sellStateStr,  cx, y, sellStateClr, sz); y += s;
      Lbl("P3Lw", StringFormat("Đỉnh SL : %s", sellMaxHigh > 0    ? DoubleToString(sellMaxHigh, _Digits) : "--"),    cx, y, clrSilver, sz); y += s;
      Lbl("P3Pk", StringFormat("Bottom  : %s", sellBottomLow > 0  ? DoubleToString(sellBottomLow, _Digits) : "--"),  cx, y, clrSilver, sz); y += s;
      Lbl("P3En", StringFormat("Entry   : %s", sellLowestPrice > 0 ? DoubleToString(sellLowestPrice, _Digits) : "--"), cx, y, clrYellow, sz); y += s;
      Lbl("P3Tp", StringFormat("TP1/TP2 : RR×%.1f / RR×%.1f", TakeProfit1, TakeProfit), cx, y, C'230,120,120', sz); y += s;
   }
   else
   {
      Lbl("P3St", "Chờ setup...", cx, y, C'60,60,80', sz); y += s;
      Lbl("P3Lw", "Trigger: wick/close vượt BB band", cx, y, C'55,55,75', sz); y += s;
      Lbl("P3Pk", "", cx, y, clrNONE, sz); y += s;
      Lbl("P3En", "", cx, y, clrNONE, sz); y += s;
      Lbl("P3Tp", StringFormat("TP1/TP2 : RR×%.1f / RR×%.1f", TakeProfit1, TakeProfit), cx, y, C'70,70,100', sz); y += s;
   }

   // ── PANEL 4: THÔNG SỐ BB + STOCHASTIC + SUPERTREND ──
   int p4Y = p3Y + p3H + 4;
   int p4H = 10 + (s+2) + (s-3) + 5*s + 8;
   CreateBG("BG4", pX, p4Y, pW, p4H, C'14,17,26', C'50,65,120');
   y = p4Y + 8;
   Lbl("I0",  "═══  THÔNG SỐ  ═══",               cx, y, C'90,140,230', sz  ); y += s+2;
   Lbl("IS",  "────────────────────────",           cx, y, C'45,58,105'        ); y += s-3;
   Lbl("IBB", StringFormat("BB      : Period=%d  Dev=%.1f", PeriodBB, Deviation),
                                                    cx, y, clrSilver,    sz   ); y += s;
   Lbl("ISF", StringFormat("Stoch   : K=%d D=%d Slow=%d  [%s]",
              StochKPeriod, StochDPeriod, StochSlowing,
              UseStochFilter ? "ON" : "OFF"),        cx, y, UseStochFilter ? clrLimeGreen : C'80,80,80', sz); y += s;
   Lbl("ISL", StringFormat("Levels  : OB=%.0f  Mid=%.0f  OV=%.0f",
              StochOverbought, StochMidLevel, StochOversold),
                                                    cx, y, clrSilver,    sz   ); y += s;
   Lbl("ISV", StringFormat("Stoch D : %.1f", stochVal),
                                                    cx, y, stochClr,     sz   ); y += s;
   Lbl("IST", StringFormat("SuperTrend: ATR=%d  Mult=%.1f  [%s]",
              STPeriod, STMult, UseSTFilter ? "ON" : "OFF"),
                                                    cx, y, UseSTFilter ? clrLimeGreen : C'80,80,80', sz); y += s;

   // ── PANEL 5: ĐIỀU KHIỂN ──
   int p5Y = p4Y + p4H + 4;
   int p5H = 10 + (s+2) + 24 + 8;
   CreateBG("BG5", pX, p5Y, pW, p5H, C'17,21,32', C'65,90,160');
   y = p5Y + 8;
   Lbl("CT",  "═══  ĐIỀU KHIỂN  ═══",              cx, y, C'90,140,230', sz  ); y += s+2;
   CreateBtn("BtnCloseAll", "  Đóng tất cả lệnh chờ", cx, y, pW - 16, 24,
             C'20,60,150', C'80,130,230');

   ChartRedraw(0);
}

//+------------------------------------------------------------------+
//|  EVENT HANDLERS                                                  |
//+------------------------------------------------------------------+

int OnInit()
{
   trade.SetExpertMagicNumber(MagicNum);
   trade.SetDeviationInPoints(10);
   trade.SetTypeFilling(ORDER_FILLING_RETURN);

   handleBB = iBands(_Symbol, _Period, PeriodBB, 0, Deviation, PRICE_CLOSE);
   if(handleBB == INVALID_HANDLE) { Print("Lỗi tạo BB handle!"); return INIT_FAILED; }

   handleStoch = iStochastic(_Symbol, _Period, StochKPeriod, StochDPeriod, StochSlowing,
                              MODE_SMA, STO_LOWHIGH);
   if(handleStoch == INVALID_HANDLE) { Print("Lỗi tạo Stochastic handle!"); return INIT_FAILED; }

   handleATR = iATR(_Symbol, _Period, STPeriod);
   if(handleATR == INVALID_HANDLE) { Print("Lỗi tạo ATR handle!"); return INIT_FAILED; }

   EventSetTimer(1);
   Print("BB Breakout EA v2.0 started.");
   return INIT_SUCCEEDED;
}

void OnDeinit(const int reason)
{
   EventKillTimer();
   ObjectsDeleteAll(0, GUI);
   if(handleBB    != INVALID_HANDLE) IndicatorRelease(handleBB);
   if(handleStoch != INVALID_HANDLE) IndicatorRelease(handleStoch);
   if(handleATR   != INVALID_HANDLE) IndicatorRelease(handleATR);
}

void OnTimer() { UpdateGUI(); }

void OnChartEvent(const int id, const long& lp, const double& dp, const string& sp)
{
   if(id != CHARTEVENT_OBJECT_CLICK) return;
   if(sp == GUI + "BtnCloseAll") CloseAllPendingOrders();
   ObjectSetInteger(0, sp, OBJPROP_STATE, false);
   UpdateGUI();
}

void OnTick()
{
   // ── Giới hạn lệnh theo phiên ──
   if(CountTradesInWindow(StartTime,  EndTime)  >= MaxTradesInWindow && IsInSession1()) CloseAllPendingOrders();
   if(CountTradesInWindow(StartTime2, EndTime2) >= MaxTradesInWindow && IsInSession2()) CloseAllPendingOrders();

   // ── Lấy dữ liệu BB + nến ──
   double open[], close[], high[], low[], upper[], lower[], middle[];
   ArraySetAsSeries(open,   true); ArraySetAsSeries(close, true);
   ArraySetAsSeries(high,   true); ArraySetAsSeries(low,   true);
   ArraySetAsSeries(upper,  true); ArraySetAsSeries(lower, true);
   ArraySetAsSeries(middle, true);

   if(CopyOpen  (_Symbol, _Period, 0, 3, open)   <= 0) return;
   if(CopyClose (_Symbol, _Period, 0, 3, close)  <= 0) return;
   if(CopyHigh  (_Symbol, _Period, 0, 3, high)   <= 0) return;
   if(CopyLow   (_Symbol, _Period, 0, 3, low)    <= 0) return;
   if(CopyBuffer(handleBB, 1, 0, 3, upper)       <= 0) return;
   if(CopyBuffer(handleBB, 2, 0, 3, lower)       <= 0) return;
   if(CopyBuffer(handleBB, 0, 0, 3, middle)      <= 0) return;

   // ── Reset khi không còn lệnh nào ──
   if(!HasAnyOrder(_Symbol) && checkBuy == 2)
   {
      checkBuy = 0; buyMinLow = 0; buyHighestPrice = 0;
      buyPeakHigh = 0; checkFirstbuy = 0; buyLowestSinceLimit = 0;
      buyTriggerTime = 0; buyTriggerCandleHigh = 0; buyTouchedMiddle = false; buyBullishPhaseLow = 0;
   }
   if(!HasAnyOrder(_Symbol) && checkSell == 2)
   {
      checkSell = 0; sellMaxHigh = 0; sellLowestPrice = 0;
      sellBottomLow = 0; checkFirstsell = 0; sellHighestSinceLimit = 0;
      sellTriggerTime = 0; sellTriggerCandleLow = 0; sellTouchedMiddle = false; sellBearishPhaseHigh = 0;
   }

   if(!IsTradingTimeVietnam()) CloseAllPendingOrders();
   if(InpNewsPauseEnabled)
   {
      UpdateNewsCache();
      if(IsInNewsZone())
      {
         ClearBuyMarks(); ClearSellMarks(); CloseAllPendingOrders();
         checkBuy  = 0; checkSell = 0;
         buyMinLow = 0; buyPeakHigh = 0; buyHighestPrice = 0; checkFirstbuy = 0;
         sellMaxHigh = 0; sellBottomLow = 0; sellLowestPrice = 0; checkFirstsell = 0;
         buyTriggerTime = 0; sellTriggerTime = 0;
         buyTriggerCandleHigh = 0; sellTriggerCandleLow = 0;
         ResetTP1Track();
         return;
      }
   }
   UpdateStopLossFromHistory();
   CheckPartialClose();
   if(PositionSelect(_Symbol)) return;

   // ── HỦY lệnh chờ nếu giá phá ngưỡng (check cả nến đóng bar[1] lẫn nến đang chạy bar[0]) ──
   if(checkBuy == 2 && HasPendingOrder(_Symbol) && (low[0] < buyMinLow || low[1] < buyMinLow))
   {
      ClearBuyMarks(); CloseAllPendingOrders();
      checkBuy = 0; buyMinLow = 0; buyHighestPrice = 0;
      buyPeakHigh = 0; checkFirstbuy = 0; buyTriggerTime = 0; buyTriggerCandleHigh = 0; buyTouchedMiddle = false; buyBullishPhaseLow = 0;
   }
   if(checkSell == 2 && HasPendingOrder(_Symbol) && (high[0] > sellMaxHigh || high[1] > sellMaxHigh))
   {
      ClearSellMarks(); CloseAllPendingOrders();
      checkSell = 0; sellMaxHigh = 0; sellLowestPrice = 0;
      sellBottomLow = 0; checkFirstsell = 0; sellTriggerTime = 0; sellTriggerCandleLow = 0; sellTouchedMiddle = false; sellBearishPhaseHigh = 0;
   }

   // ── TRACKING BUY ──
   if(checkBuy == 1)
   {
      // low[1] phá xuống buyMinLow → đáy đầu tiên bị thủng, setup false, reset hoàn toàn
      if(buyMinLow > 0.0 && low[1] < buyMinLow)
      {
         ClearBuyMarks();
         checkBuy = 0; checkFirstbuy = 0; buyMinLow = 0; buyPeakHigh = 0;
         buyHighestPrice = 0; buyTriggerTime = 0; buyTriggerCandleHigh = 0;
         buyTouchedMiddle = false; buyBullishPhaseLow = 0;
      }
      else
      {

      if(buyMinLow == 0.0 || low[1] < buyMinLow) buyMinLow = low[1];

      // Track nếu nến đóng cửa qua đường giữa
      if(close[1] >= middle[1]) buyTouchedMiddle = true;

      // Đỉnh đã xác nhận nhưng giá đóng trên đường giữa → setup vô hiệu, reset hoàn toàn
      if(checkFirstbuy == 1 && close[1] >= middle[1])
      {
         ClearBuyMarks();
         checkBuy = 0; buyMinLow = 0; buyHighestPrice = 0;
         buyPeakHigh = 0; checkFirstbuy = 0; buyTriggerTime = 0;
         buyTriggerCandleHigh = 0; buyTouchedMiddle = false; buyBullishPhaseLow = 0;
      }
      else if(low[0] < buyMinLow)
      {
         if(buyTouchedMiddle)
         {
            // Case 2: reset hoàn toàn
            ClearBuyMarks();
            checkBuy = 0; buyMinLow = 0; buyHighestPrice = 0;
            buyPeakHigh = 0; checkFirstbuy = 0; buyTriggerTime = 0;
            buyTriggerCandleHigh = 0; buyTouchedMiddle = false;
         }
         else
         {
            // Case 1: restart trigger mới, không cần nến ngoài band
            ClearBuyMarks();
            buyTriggerTime       = iTime(_Symbol, _Period, 0);
            buyTriggerCandleHigh = high[0];
            buyMinLow            = low[0];
            checkFirstbuy        = 0; buyPeakHigh = 0; buyTouchedMiddle = false;
            // Vẽ trigger mới
            DrawBandMark("BT" + IntegerToString((long)buyTriggerTime),
                         buyTriggerTime, low[0] - GetSpread() * 3, "▲", C'50,205,50', 11, ANCHOR_LOWER);
         }
      }
      else
      {
         // Nến tăng phải phá lên trên HIGH của nến trigger mới xác nhận đáy
         if(checkFirstbuy == 0 && close[1] > open[1] && high[1] > buyTriggerCandleHigh)
         {
            if(close[1] <= middle[1])
            {
               // Đỉnh hợp lệ: close dưới đường giữa
               checkFirstbuy = 1; buyPeakHigh = high[1];
               buyBullishPhaseLow = low[1];
               buyPeakMarkTime = iTime(_Symbol, _Period, 1);
               DrawBandMark("BPK" + IntegerToString((long)buyPeakMarkTime),
                            buyPeakMarkTime, buyPeakHigh + GetSpread() * 3, "▼", clrOrange, 9, ANCHOR_UPPER);
            }
            else
            {
               // Nến tăng đóng cửa qua đường giữa → setup vô hiệu, reset hoàn toàn
               ClearBuyMarks();
               checkBuy = 0; buyMinLow = 0; buyHighestPrice = 0;
               buyPeakHigh = 0; checkFirstbuy = 0; buyTriggerTime = 0;
               buyTriggerCandleHigh = 0; buyTouchedMiddle = false; buyBullishPhaseLow = 0;
            }
         }
         if(checkFirstbuy == 1 && close[1] > open[1])
         {
            if(high[1] > buyPeakHigh) buyPeakHigh = high[1];
            // Cập nhật đáy thấp nhất của toàn bộ phase tăng
            if(buyBullishPhaseLow == 0.0 || low[1] < buyBullishPhaseLow)
               buyBullishPhaseLow = low[1];
         }
      }
      } // else (checkFirstbuy reset)
   }

   // ── TRACKING SELL ──
   if(checkSell == 1)
   {
      // high[1] phá lên sellMaxHigh → đỉnh đầu tiên bị thủng, setup false, reset hoàn toàn
      if(sellMaxHigh > 0.0 && high[1] > sellMaxHigh)
      {
         ClearSellMarks();
         checkSell = 0; checkFirstsell = 0; sellMaxHigh = 0; sellBottomLow = 0;
         sellLowestPrice = 0; sellTriggerTime = 0; sellTriggerCandleLow = 0;
         sellTouchedMiddle = false; sellBearishPhaseHigh = 0;
      }
      else
      {

      if(sellMaxHigh == 0.0 || high[1] > sellMaxHigh) sellMaxHigh = high[1];

      // Track nếu nến đóng cửa qua đường giữa
      if(close[1] <= middle[1]) sellTouchedMiddle = true;

      // Đáy đã xác nhận nhưng giá đóng dưới đường giữa → setup vô hiệu, reset hoàn toàn
      if(checkFirstsell == 1 && close[1] <= middle[1])
      {
         ClearSellMarks();
         checkSell = 0; sellMaxHigh = 0; sellLowestPrice = 0;
         sellBottomLow = 0; checkFirstsell = 0; sellTriggerTime = 0;
         sellTriggerCandleLow = 0; sellTouchedMiddle = false; sellBearishPhaseHigh = 0;
      }
      else if(high[0] > sellMaxHigh)
      {
         if(sellTouchedMiddle)
         {
            // Case 2: reset hoàn toàn
            ClearSellMarks();
            checkSell = 0; sellMaxHigh = 0; sellLowestPrice = 0;
            sellBottomLow = 0; checkFirstsell = 0; sellTriggerTime = 0;
            sellTriggerCandleLow = 0; sellTouchedMiddle = false;
         }
         else
         {
            // Case 1: restart trigger mới
            ClearSellMarks();
            sellTriggerTime      = iTime(_Symbol, _Period, 0);
            sellTriggerCandleLow = low[0];
            sellMaxHigh          = high[0];
            checkFirstsell       = 0; sellBottomLow = 0; sellTouchedMiddle = false;
            // Vẽ trigger mới
            DrawBandMark("ST" + IntegerToString((long)sellTriggerTime),
                         sellTriggerTime, high[0] + GetSpread() * 3, "▼", C'220,60,60', 11, ANCHOR_UPPER);
         }
      }
      else
      {
         // Nến giảm phải phá xuống dưới LOW của nến trigger mới xác nhận đỉnh
         if(checkFirstsell == 0 && close[1] < open[1] && low[1] < sellTriggerCandleLow)
         {
            if(close[1] >= middle[1])
            {
               // Đáy hợp lệ: close trên đường giữa
               checkFirstsell = 1; sellBottomLow = low[1];
               sellBearishPhaseHigh = high[1];
               sellBottomMarkTime = iTime(_Symbol, _Period, 1);
               DrawBandMark("SBT" + IntegerToString((long)sellBottomMarkTime),
                            sellBottomMarkTime, sellBottomLow - GetSpread() * 3, "▲", clrDodgerBlue, 11, ANCHOR_LOWER);
            }
            else
            {
               // Nến giảm đóng cửa qua đường giữa → setup vô hiệu, reset hoàn toàn
               ClearSellMarks();
               checkSell = 0; sellMaxHigh = 0; sellLowestPrice = 0;
               sellBottomLow = 0; checkFirstsell = 0; sellTriggerTime = 0;
               sellTriggerCandleLow = 0; sellTouchedMiddle = false; sellBearishPhaseHigh = 0;
            }
         }
         if(checkFirstsell == 1 && close[1] < open[1])
         {
            if(low[1] < sellBottomLow) sellBottomLow = low[1];
            // Cập nhật đỉnh cao nhất của toàn bộ phase giảm
            if(sellBearishPhaseHigh == 0.0 || high[1] > sellBearishPhaseHigh)
               sellBearishPhaseHigh = high[1];
         }
      }
      } // else (checkFirstsell reset)
   }

   // ── BUY ENTRY: nến 2 tăng → nến 1 giảm xác nhận đỉnh ──
   if(checkBuy == 1 && checkFirstbuy == 1 && !HasAnyOrder(_Symbol))
   {
      bool buyPhaseConfirm = (buyBullishPhaseLow == 0.0 || close[1] < buyBullishPhaseLow);
      if(close[2] > open[2] && close[1] < open[1] && buyPeakHigh > 0 && buyPhaseConfirm
         && low[1] >= buyMinLow)   // nến xác nhận không được phá đáy đầu tiên
      {
         double peakHigh = MathMax(buyPeakHigh, high[2]);
         if(close[2] <= middle[2])
         {
            int stochBars = (buyTriggerTime > 0)
                            ? MathMax(2, iBarShift(_Symbol, _Period, buyTriggerTime))
                            : 9;
            bool stochOK = !UseStochFilter || CheckStochBuy(stochBars);

            bool stOK = true;
            if(UseSTFilter)
            {
               double stU, stL;
               if(GetSTBands(2, stU, stL))
                  stOK = (buyPeakHigh <= stU);
            }

            if(stochOK && stOK)
            {
               double entry = NormalizeDouble(peakHigh + GetSpread(), _Digits);
               double sl    = NormalizeDouble(buyMinLow, _Digits);
               double lot   = CalculateLotBySize(entry, sl);
               if(trade.BuyStop(lot, entry, _Symbol, 0, 0, ORDER_TIME_GTC, 0, "BB Buy"))
               {
                  checkBuy = 2; buyHighestPrice = peakHigh;
                  Print("BuyStop @ ", entry, " | SL @ ", sl);
               }
            }
         }
      }
   }

   // ── SELL ENTRY: nến 2 giảm → nến 1 tăng xác nhận đáy ──
   if(checkSell == 1 && checkFirstsell == 1 && !HasAnyOrder(_Symbol))
   {
      bool sellPhaseConfirm = (sellBearishPhaseHigh == 0.0 || close[1] > sellBearishPhaseHigh);
      if(close[2] < open[2] && close[1] > open[1] && sellBottomLow > 0 && sellPhaseConfirm
         && high[1] <= sellMaxHigh)   // nến xác nhận không được phá đỉnh đầu tiên
      {
         double bottomLow = MathMin(sellBottomLow, low[2]);
         if(close[2] >= middle[2])
         {
            int stochBars = (sellTriggerTime > 0)
                            ? MathMax(2, iBarShift(_Symbol, _Period, sellTriggerTime))
                            : 9;
            bool stochOK = !UseStochFilter || CheckStochSell(stochBars);

            bool stOK = true;
            if(UseSTFilter)
            {
               double stU, stL;
               if(GetSTBands(2, stU, stL))
                  stOK = (sellBottomLow >= stL);
            }

            if(stochOK && stOK)
            {
               double entry = NormalizeDouble(bottomLow, _Digits);
               double sl    = NormalizeDouble(sellMaxHigh + GetSpread(), _Digits);
               double lot   = CalculateLotBySize(entry, sl);
               if(trade.SellStop(lot, entry, _Symbol, 0, 0, ORDER_TIME_GTC, 0, "BB Sell"))
               {
                  checkSell = 2; sellLowestPrice = bottomLow;
                  Print("SellStop @ ", entry, " | SL @ ", sl);
               }
            }
         }
      }
   }

   // ── TRIGGER BUY: nến quét dưới Lower BB ──
   if(low[1] < lower[1] && close[1] < middle[1] && checkBuy == 0 && checkSell == 0)
   {
      checkBuy = 1; buyMinLow = low[1];
      buyPeakHigh = 0; buyHighestPrice = 0; checkFirstbuy = 0;
      buyTriggerTime = iTime(_Symbol, _Period, 1);
      buyTriggerCandleHigh = high[1];
      buyTouchedMiddle = false; buyBullishPhaseLow = 0;
      // ↑ Đánh dấu đáy khi chạm Lower BB
      DrawBandMark("BT" + IntegerToString((long)buyTriggerTime),
                   buyTriggerTime, low[1] - GetSpread() * 3, "▲", C'50,205,50', 11, ANCHOR_LOWER);
   }

   // ── TRIGGER SELL: nến quét trên Upper BB ──
   if(high[1] > upper[1] && close[1] > middle[1] && checkSell == 0 && checkBuy == 0)
   {
      checkSell = 1; sellMaxHigh = high[1];
      sellBottomLow = 0; sellLowestPrice = 0; checkFirstsell = 0;
      sellTriggerTime = iTime(_Symbol, _Period, 1);
      sellTriggerCandleLow = low[1];
      sellTouchedMiddle = false; sellBearishPhaseHigh = 0;
      // ↓ Đánh dấu đỉnh khi chạm Upper BB
      DrawBandMark("ST" + IntegerToString((long)sellTriggerTime),
                   sellTriggerTime, high[1] + GetSpread() * 3, "▼", C'220,60,60', 11, ANCHOR_UPPER);
   }
}

//+------------------------------------------------------------------+
//|  TRADE TRANSACTION                                               |
//+------------------------------------------------------------------+

void OnTradeTransaction(const MqlTradeTransaction &trans,
                        const MqlTradeRequest     &request,
                        const MqlTradeResult      &result)
{
   if(trans.type != TRADE_TRANSACTION_DEAL_ADD) return;
   if(trans.deal == 0) return;
   if(!HistoryDealSelect(trans.deal)) return;
   if(HistoryDealGetString(trans.deal,  DEAL_SYMBOL) != _Symbol) return;
   if(HistoryDealGetInteger(trans.deal, DEAL_MAGIC)  != (long)MagicNum) return;
   if(HistoryDealGetInteger(trans.deal, DEAL_ENTRY)  != DEAL_ENTRY_OUT) return;

   long   reason = HistoryDealGetInteger(trans.deal, DEAL_REASON);
   double profit = HistoryDealGetDouble(trans.deal,  DEAL_PROFIT);

   if(reason == DEAL_REASON_TP || profit > 0)
   {
      ClearBuyMarks(); ClearSellMarks();
      checkBuy  = 0; checkSell = 0;
      buyHighestPrice = 0; buyLowestSinceLimit = 0; buyMinLow = 0; checkFirstbuy = 0;
      sellLowestPrice = 0; sellHighestSinceLimit = 0; sellMaxHigh = 0; checkFirstsell = 0;
      buyPeakHigh = 0; sellBottomLow = 0;
      buyTriggerTime = 0; sellTriggerTime = 0;
      buyTriggerCandleHigh = 0; sellTriggerCandleLow = 0;
      buyTouchedMiddle = false; sellTouchedMiddle = false;
      buyBullishPhaseLow = 0; sellBearishPhaseHigh = 0;
      ResetTP1Track();
      CloseAllPendingOrders();
   }

   UpdateGUI();
}
