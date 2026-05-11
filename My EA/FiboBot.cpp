//+------------------------------------------------------------------+
//|                   LQT Expert Advisors                     |
//|                   Copyright 2025, The Trader                     |
//|                   Version 1.00 - Description                     |
//+------------------------------------------------------------------+

#include <Trade\Trade.mqh>
#include <ChartObjects\ChartObjectsTxtControls.mqh>
//Enum
enum PriceType {
   HIGH,
   CLOSE,
   LOW
};

enum TradeType {
   LONGTRADE,
   SHORTTRADE
};

enum SymbolInfFieldName {
   SYMBOL_NAME,
   ADDITIONALSTOPLOSS,
   MINSTOPLOSS,
   MAXSTOPLOSS,
   MIN236FIBO,
   ISBESTOPLOSS,
   ISCHECKSETUP500,
   PADDINGEMA,
   ISCHECKCROSSCANDLE,
   PADDINGENDFIBO,
   ISCHECKBREAKOUT
};

enum StructType {
   SPH, // Small Pivot High
   SPL, // Small Pivot Low
   LPH, // Large Pivot High
   LPL, // Large Pivot Low
   NONE
};

// Struct
struct StructPoint {
   int index;
   double point;
   StructType type;
};

struct Order {
   double stopLoss;
   double takeProfit;
};

struct TicketOrder {
   int numericalOrderFibo236;
   int numericalOrderFibo500;
   ulong ticket;
   double startPoint;
   double endPoint;
};

struct SymbolInf {
   string name;
   double additionalStopLoss;
   double minStopLoss;
   double maxStopLoss;
   double min236Fibo;
   bool beStopLoss;
   bool isCheckSetup500;
   double paddingEma;
   bool isCheckCrossCandle;
   double paddingEndFibo;
   bool isCheckBreakOut;
};

struct Fibo {
   double startPoint;
   int startIndex;
   double endPoint;
   int endIndex;
   string trend;
   bool priceExceeds500;
   int priceExceeds500Index;
   bool priceExceeds236;
   bool priceExceedsEma;
   bool priceH1Exceeds382;
   Order order;
};
// Input data
input double RiskLongTrade = 100; // Rủi ro long trade (USD)
input double RiskShortTrade = 50; // Rủi ro short trade (USD)
input bool DrawStruct = true; // Vẽ cấu trúc thị trường
input bool DrawFibo = true; // Vẽ Fibo
input bool InpFridayCloseTrades = true;  // FridayCloseTrades
// Constant data
const int ZERO = 0;
const int ONE = 1;
const int TWO = 2;
const int THREE = 3;
const int PERIOD_EMA = 25;
const string BUY = "BUY";
const string SELL = "SELL";
const int FIBO_236 = 236;
const int FIBO_382 = 368;
const int FIBO_500 = 500;
const int FIBO_618 = 618;
// Quantity
const int CANDLES_M15_30_DAYS = 2880;
const int CANDLES_M15_3_DAYS = 288;
const int CANDLES_H4_7_DAYS = 42;
const int TWENTY_CANDLES = 20;
const int CANDLES_CHECK_EMA_H1 = 11;
const int CANDLES_CHECK_EMA_M15 = 16;
// Symbol informations
const SymbolInf symbolInf[] = {
   {"XAUUSD", 500, 2700, 8000, 3000, false, true, 50, true, 8, false},
   {"GBPJPY", 35, 200, 500, 400, false, true, 0, false, 8, false}, 
   {"EURUSD", 20, 70, 300, 150, true, true, 3, false, 8, true},
   {"GBPUSD", 20, 70, 300, 150, true, true, 3, false, 8, true},
   {"AUDUSD", 20, 70, 300, 150, true, true, 0, true, 8, true},
   {"NZDUSD", 20, 70, 300, 100, true, true, 0, true, 8, true},
   {"USDCAD", 20, 85, 300, 150, true, true, 3, false, 8, true},
   {"USDCHF", 20, 85, 300, 150, true, true, 3, false, 8, true},
};
// Biến toàn cục
CTrade Trade;
datetime CandleCloseTime;// Biến kiểm tra giá chạy 15p một lần 
double BEPrice; // Biến giá để BE
TicketOrder MyTicketOrder[5];// Mảng order
// Function
string getTypeTrend();
string getMainTrend(datetime targetTime);
double getFiboData(double start, double end, int markerFibo);
void getCandleHighest(int start, int end, Fibo &fibo, MqlRates &rates[]);
void getCandleLowest(int start, int end, Fibo &fibo, MqlRates &rates[]);
void getCandleHighestOrLowest(int start, int end, Fibo &fibo, MqlRates &rates[]);
double getStopLossM15(Fibo &fibo, int shift, int markerFibo);
int getNextIndexEmptyFiboArray(const Fibo &fiboArray[]);
int getNextIndexEmptyOrderArray();
double getLotSize(double risk, double stopLossPoint);
double getPriceLowest(int start, int end, PriceType priceType, bool isGetIndex);
double getPriceHighest(int start, int end, PriceType priceType, bool isGetIndex);
void getStructField(const SymbolInfFieldName fieldName);
StructPoint getStructPoint(StructPoint &structPointArray[], StructType typePoint);
int getIndexPointLow(StructPoint &structPointArray[], int start, int end);
int getIndexPointHigh(StructPoint &structPointArray[], int start, int end);
int getTakeProfitRatio(double stopLoss, double entry);
void setStructPointArray(int checkIndex, StructPoint &myArray[], int candles, ENUM_TIMEFRAMES tf);
void checkFiboNextCandle(Fibo &fibo, int index, Fibo &fiboArray[], MqlRates &rates[]);
void checkNextCandle(Fibo &fibo);
void checkSetup(Fibo &fibo, MqlRates &rates[], int ticketOrderIndex);
bool isDuplicate(const Fibo &a, const Fibo &b);
bool isCloseH1Fibo618(MqlRates &rates[], Fibo &fibo, int index);
bool isCheckH1Fibo(Fibo &fibo, datetime time_m15);
bool isTrendCandle(MqlRates &candle, MqlRates &pre_candle, Fibo &fibo, int markerFibo, int index);
bool isEmaConditions(datetime targetTime, string typeTrend);
bool isEmaH4PreConditions(datetime candleTime, string trend);
bool isConditions(StructPoint &structPointArray[], MqlRates &rates[], Fibo &fibo, int index, int markerFibo);
bool isResetFibo(Fibo &fibo, MqlRates &rates[], int index);
bool isFiboCondition(Fibo &fibo);
bool isCheckMomentum(MqlRates &rates[], Fibo &fibo, int index, int markerFibo);
bool isOrderWithSameOrder(double stopLossOrder, datetime time, string comment);
bool isNosdCandle(const MqlRates &rates[], string trend, int index);
bool isLongWickCandle(const MqlRates &rates[], Fibo &fibo, int index);
bool isFollowFiboFun(Fibo &fibo, const MqlRates &rates[], int index, double fibo236);
bool isStrongReversalCandle(MqlRates &rates[], Fibo &fibo);
bool isReversalPatterns(MqlRates &rates[], string trend, int index);
bool isGapInRange(int start, int end, const MqlRates &rates[]);
bool isCheckSwingPoint(MqlRates &rates[], Fibo &fibo, StructPoint &structPointArray[], int index, int markerFibo);
bool isDuplicateInArray(Fibo &fiboArray[]);
bool isSideWay(StructPoint &structPointArray[], Fibo &fibo, MqlRates &rates[]);
bool isSoFar(Fibo &fibo, MqlRates &rates[], double stopLossPoint);
bool isH1Reverse(Fibo &fibo, MqlRates &rates[]);
bool isSetupBreakFibo(Fibo &fibo, MqlRates &rates[]);
bool isBreakPullback(Fibo &fibo, MqlRates &rates[]);
bool isEmaPreConditions(string trend, int candleCheck, ENUM_TIMEFRAMES timeFrame);
void updateFiboEndSell(Fibo &fibo, const MqlRates &rates[], int index);
void updateFiboEndBuy(Fibo &fibo, const MqlRates &rates[], int index);
void setDefaultStatusFibo(Fibo &fibo);
void setFibo(Fibo &fibo, int days, Fibo &fiboArray[]);
void setOrder(StructPoint &structPointArray[], MqlRates &candle, Fibo &fibo, string comment, int ratio, TradeType tradeType);
void resetFibo(Fibo &fibo, int &start_daw_fibo, MqlRates &rates[]);
void addFiboToArray(Fibo &newFibo, Fibo &fiboArray[]);
void addOrderToArray(ulong ticket, Fibo &fibo, string comment);
void drawFiboInChart(Fibo &fiboArray[]);
void drawStruct(StructPoint &point);
void handlePriceExceeds(Fibo &fibo, MqlRates &rates[], int index, bool &isFollowFibo);
void managePositions();
void modifyStopLoss(ulong ticket, double newStopLoss, double takeProfit);
double calculateAverage(StructPoint &structPointArray[], StructType typePoint, int start);
double calculatePotentialLoss(ulong ticket);

int OnInit(){
   EventSetTimer(ONE);
   return (INIT_SUCCEEDED);
}

void OnDeinit(const int reason){
   EventKillTimer();
}

void OnChartEvent(const int id, const long &lparam, const double &dparam, const string &sparam){
   if(id == CHARTEVENT_CHART_CHANGE) drawStruct(); // Chạy lại hàm chính khi có sự kiện chart change
}

void OnTick(){
   managePositions();
}

void OnTimer(){
   if(InpFridayCloseTrades && PositionsTotal() > 0 && fridayTimeIsActive()) closeAllPositions();
   // Check current time and next M15 candle close time
   datetime currentTime = TimeCurrent();
   datetime currentCandleCloseTime = iTime(_Symbol, PERIOD_M15, 1) + PeriodSeconds(PERIOD_M15);
   
   datetime closeTime = iTime(_Symbol, PERIOD_CURRENT, 0) + PeriodSeconds(PERIOD_CURRENT);
   string timeString = TimeToString(closeTime - TimeCurrent(), TIME_SECONDS);
   Comment("Count down time: ", timeString, "\n", "\n",
            "Risk long trade: ", DoubleToString(RiskLongTrade, TWO), " USD", "\n",
            "Draw Fibo: ", DrawFibo ? "TRUE" : "FALSE", "\n",
            "Draw Structure: ", DrawStruct ? "TRUE" : "FALSE", "\n");
   
   bool isRunningEa = false; 
   
   if(currentCandleCloseTime != CandleCloseTime &&
      currentCandleCloseTime <= currentTime ){
      CandleCloseTime = currentCandleCloseTime;
      isRunningEa = true;
   }
   if (isRunningEa){
      Fibo fiboArray[10];
      for (int index = 0; index < 10; index++){
         setDefaultFibo(fiboArray[index]);
      }
      // Set default fibo
      setFibo(fiboArray[0], CANDLES_M15_30_DAYS, fiboArray);
      for(int orderIndex = 0; orderIndex < 5; orderIndex ++){
         bool isDuplicate = false;
         for(int fiboIndex = 0; fiboIndex < 10; fiboIndex ++){
            if(fiboArray[fiboIndex].startPoint == 0.0 
               && fiboArray[fiboIndex].endPoint == 0.0)
               continue;
            if(fiboArray[fiboIndex].startPoint == MyTicketOrder[orderIndex].startPoint
               && fiboArray[fiboIndex].endPoint == MyTicketOrder[orderIndex].endPoint){
               isDuplicate = true;
               break;
            }
         } 
         if(!isDuplicate){
            MyTicketOrder[orderIndex].endPoint = 0.0;
            MyTicketOrder[orderIndex].startPoint = 0.0;
            MyTicketOrder[orderIndex].ticket = 0;
            MyTicketOrder[orderIndex].numericalOrderFibo236 = 0;
            MyTicketOrder[orderIndex].numericalOrderFibo500 = 0;
         }
      }
      isRunningEa = false;
      
      if(DrawFibo) drawFiboInChart(fiboArray);
      if(DrawStruct) drawStruct();
      ChartRedraw(0);
   }
}

void setFibo(Fibo &fibo, int days, Fibo &fiboArray[]){
   fibo.trend = getTypeTrend();
   // Get the value of M15 candle within days
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   int copied = CopyRates(Symbol(), PERIOD_M15, 0, days, rates);
   
   if (copied > 0){
      // Take the value of the highest or lowest M15 candle
      if (fibo.trend == SELL){ 
         getCandleHighest(copied - 1, 0, fibo, rates);  
      } else if (fibo.trend == BUY){
         getCandleLowest(copied - 1, 0, fibo, rates);
      }
      
      // Browse next M15 candles
      int startDawFibo = fibo.endIndex - 1;
      checkFiboNextCandle(fibo, startDawFibo, fiboArray, rates);
   }
}

void checkFiboNextCandle(Fibo &fibo, int index, Fibo &fiboArray[], MqlRates &rates[]){
   bool isRecursive = true;
   
   while(index >= 1){
      if(fibo.endIndex - index >= CANDLES_M15_3_DAYS && isRecursive){
         Fibo newFibo;
         setDefaultFibo(newFibo);
         //update new fibo
         newFibo.startIndex = fibo.endIndex;
         newFibo.endIndex = fibo.endIndex;
         
         if(fibo.trend == SELL){
            newFibo.startPoint = fibo.endPoint;
            newFibo.endPoint = rates[fibo.endIndex].high;
            newFibo.trend = BUY;
         } else if(fibo.trend == BUY){
            newFibo.startPoint = fibo.endPoint;
            newFibo.endPoint = rates[fibo.endIndex].low;
            newFibo.trend = SELL;
         }
         checkNextCandle(newFibo);
         
         checkFiboNextCandle(newFibo,newFibo.endIndex - 1, fiboArray, rates);
         addFiboToArray(newFibo, fiboArray);
            
         isRecursive = false;
      }
      // Update end points
      bool isFollowFibo = false;
      handlePriceExceeds(fibo, rates, index, isFollowFibo);
      if (isFollowFibo){
         index = fibo.startIndex - 1;
         continue;
      }
            
      // Reset fibo if conditions are met
      if(isResetFibo(fibo, rates, index)){
         resetFibo(fibo, index, rates);
         continue;
      }
                  
      // Xóa trùng lặp
      if(index == ONE && isDuplicateInArray(fiboArray)){
         break;
      }
      // Kiểm tra độ dài fibo
      if(index == ONE && isFiboCondition(fibo)){
         bool isSetupRunning = true;
         int ticketOrderIndex = -1;
         for(int orderIndex = 0; orderIndex < 5; orderIndex++){
            if((MyTicketOrder[orderIndex].endPoint == fibo.endPoint) 
               && (MyTicketOrder[orderIndex].startPoint == fibo.startPoint)){
               if(PositionSelectByTicket(MyTicketOrder[orderIndex].ticket) 
                  && MyTicketOrder[orderIndex].ticket){
                  isSetupRunning = false;
                  break;
               }
               ticketOrderIndex = orderIndex;
            }
         }
         if(isSetupRunning){
            checkSetup(fibo, rates, ticketOrderIndex);
         }
      }
      
      if(fibo.trend == SELL && rates[index].low < fibo.endPoint){
         updateFiboEndSell(fibo, rates, index);
      } else if(fibo.trend == BUY && rates[index].high > fibo.endPoint){
         updateFiboEndBuy(fibo, rates, index);
      }
      
      index--;
   }
}

bool isResetFibo(Fibo &fibo, MqlRates &rates[], int index){
   if(fibo.trend == SELL && rates[index].high > fibo.startPoint) return true;
   if(fibo.trend == BUY && rates[index].low < fibo.startPoint) return true;
   
   if(fibo.priceExceeds500 && isCloseH1Fibo618(rates, fibo, index)) return true;
   
   return false;
}

void updateFiboEndSell(Fibo &fibo, const MqlRates &rates[], int index){
   fibo.endPoint = rates[index].low;
   fibo.endIndex = index;
   
   setDefaultStatusFibo(fibo);
}

void updateFiboEndBuy(Fibo &fibo, const MqlRates &rates[], int index){
   fibo.endPoint = rates[index].high;
   fibo.endIndex = index;
   
   setDefaultStatusFibo(fibo);
}

void handlePriceExceeds(Fibo &fibo, MqlRates &rates[], int index, bool &isFollowFibo){
   // Tính toán giá trị Fibonacci 23.6% và 50.0% một lần
   double fibo236 = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_236);
   double fibo500 = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_500);
   // Tính toán giá trị EMA M15
   double emaM15[];
   ArraySetAsSeries(emaM15, true);
   int handleM15 = iMA(_Symbol, PERIOD_M15, PERIOD_EMA, 0, MODE_EMA, PRICE_CLOSE);
   // Kiểm tra giá hiện tại so với mức Fibonacci
   if(fibo.trend == SELL){
      if(rates[index].high >= fibo236){
         if(!fibo.priceExceeds236)
            fibo.priceExceeds236 = true;
      }
      if(rates[index].high >= fibo500){
         if(!fibo.priceExceeds500){
            fibo.priceExceeds500Index = index;
            fibo.priceExceeds500 = true;
         }
      }
      //Kiểm tra h1 382
      if(!fibo.priceH1Exceeds382){
         fibo.priceH1Exceeds382 = isCheckH1Fibo(fibo, rates[index].time, FIBO_382);
      }
      // Kiểm tra giá quay về ema25
      if(handleM15 >= 0 && CopyBuffer(handleM15, 0, index, 1, emaM15) > 0){
         if(!fibo.priceExceedsEma)
            fibo.priceExceedsEma = emaM15[0] < rates[index].high;
      }
   } else if(fibo.trend == BUY){
      if(rates[index].low <= fibo236){
         if(!fibo.priceExceeds236)
            fibo.priceExceeds236 = true;
      }
      if(rates[index].low <= fibo500){
         if(!fibo.priceExceeds500){
            fibo.priceExceeds500Index = index;
            fibo.priceExceeds500 = true;
         }
      }
      //Kiểm tra h1 382
      if(!fibo.priceH1Exceeds382){
         fibo.priceH1Exceeds382 = isCheckH1Fibo(fibo, rates[index].time, FIBO_382);
      }
      // Kiểm tra giá quay về ema25   
      if(handleM15 >= 0 && CopyBuffer(handleM15, 0, index, 1, emaM15) > 0){
         if(!fibo.priceExceedsEma)
            fibo.priceExceedsEma = emaM15[0] > rates[index].low; 
      }
   }
   if(fibo.priceExceeds500){
      if(isFollowFiboFun(fibo, rates, index, fibo236)){
        int startIndex = (fibo.endIndex == fibo.startIndex) ? fibo.endIndex - 1 : fibo.endIndex;
        getCandleHighestOrLowest(startIndex, index, fibo, rates);
        isFollowFibo = true;
      }
   }
}

void getCandleHighestOrLowest(int start, int end, Fibo &fibo, MqlRates &rates[]){
   if(fibo.trend == SELL){
      getCandleHighest(start, end, fibo, rates);
   } else if(fibo.trend == BUY){
      getCandleLowest(start, end, fibo, rates);
   }

   checkNextCandle(fibo);
   setDefaultStatusFibo(fibo);
}

string getTypeTrend(){
   int pointBullishIndex = 0, pointBearishIndex = 0;
   
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   int copied = CopyRates(Symbol(), PERIOD_M15, 0, CANDLES_M15_30_DAYS, rates);
    
   double lowestPoint = rates[0].low, highestPoint = rates[0].high; 
   
   for(int index = 1; index <= copied - 1; index++){
      
      // Kiểm tra nếu cây nến tăng giá (giá đóng cửa > giá mở cửa)
      if(lowestPoint >= rates[index].low){
         lowestPoint = rates[index].low;
         pointBullishIndex = index;
      } 
      if(highestPoint <= rates[index].high){
         highestPoint = rates[index].high;
         pointBearishIndex = index;
      } 
   }
   if(pointBearishIndex > pointBullishIndex){
      return SELL;
   } else return BUY;
}

double getFiboData(double start, double end, int markerFibo){   
   if(start == 0 && end == 0) return 0;
   
   double multiplier = 0.0;
   if(markerFibo) multiplier = (double)markerFibo/1000;
   
   return NormalizeDouble((start - end) * multiplier + end, _Digits);
}

bool isCloseH1Fibo618(MqlRates &rates[], Fibo &fibo, int index){
   double priceFibo = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_618);
   int count = 0;
   for(int i = index; i < fibo.endIndex; i++){
      if(rates[i].close > priceFibo && fibo.trend == SELL) count++;
      if(rates[i].close < priceFibo && fibo.trend == BUY) count++;
   }

   return count > THREE;
}

void getCandleHighest(int start, int end, Fibo &fibo, MqlRates &rates[]){
   int result = (int)getPriceHighest(start, end, HIGH, true);
   
   fibo.startPoint = rates[result].high;
   fibo.startIndex = result;
   if(rates[result].open < rates[result].close){
      fibo.endPoint = rates[result].close;
   } else fibo.endPoint = rates[result].low;
   fibo.endIndex = result;
}

void getCandleLowest(int start, int end, Fibo &fibo, MqlRates &rates[]){
   int result = (int)getPriceLowest(start, end, LOW, true);
   
   fibo.startPoint = rates[result].low;
   fibo.startIndex = result;
   if(rates[result].open > rates[result].close){
      fibo.endPoint = rates[result].close;
   } else fibo.endPoint = rates[result].high;
   fibo.endIndex = result;
}

void checkNextCandle(Fibo &fibo){
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   int copied = CopyRates(Symbol(), PERIOD_M15, 0, fibo.startIndex + 1, rates);
   int nextCandleIndex = fibo.startIndex - 1;
   
   while(nextCandleIndex >= 0){
      if(fibo.trend == BUY && 
         (rates[nextCandleIndex].low < getFiboData(fibo.startPoint, fibo.endPoint, FIBO_500)
            || rates[fibo.startIndex].close < getFiboData(fibo.startPoint, fibo.endPoint, FIBO_500))){
          
         if(rates[nextCandleIndex].close < rates[nextCandleIndex].open){
            fibo.startPoint = rates[nextCandleIndex].high;
            fibo.startIndex = nextCandleIndex;
            fibo.endPoint = rates[nextCandleIndex].low;
            fibo.endIndex = nextCandleIndex;
            fibo.trend = SELL;
            continue;
         } 
         
         fibo.startPoint = rates[nextCandleIndex].low;
         fibo.startIndex = nextCandleIndex;
         fibo.endPoint = rates[nextCandleIndex].high;
         fibo.endIndex = nextCandleIndex;
         
         nextCandleIndex--;
         continue;
      }
      else if(fibo.trend == SELL && 
            (rates[nextCandleIndex].high > getFiboData(fibo.startPoint, fibo.endPoint, FIBO_500)
            || rates[fibo.startIndex].close > getFiboData(fibo.startPoint, fibo.endPoint, FIBO_500))){
         
         if(rates[nextCandleIndex].close > rates[nextCandleIndex].open){
            fibo.startPoint = rates[nextCandleIndex].low;
            fibo.startIndex = nextCandleIndex;
            fibo.endPoint = rates[nextCandleIndex].high;
            fibo.endIndex = nextCandleIndex;
            fibo.trend = BUY;
            continue;
         }
         fibo.startPoint = rates[nextCandleIndex].high;
         fibo.startIndex = nextCandleIndex;
         fibo.endPoint = rates[nextCandleIndex].low;
         fibo.endIndex = nextCandleIndex;
         
         nextCandleIndex--;
         continue;
      }
      break;
   }
}

void resetFibo(Fibo &fibo, int &startDawFibo, MqlRates &rates[]){
   if(fibo.trend == SELL){
      fibo.trend = BUY;
      getCandleLowest(fibo.endIndex, startDawFibo, fibo, rates);
   } else if(fibo.trend == BUY){
      fibo.trend = SELL;
      getCandleHighest(fibo.endIndex, startDawFibo, fibo, rates);    
   }
   
   checkNextCandle(fibo);
   setDefaultStatusFibo(fibo);
   
   startDawFibo = fibo.endIndex - 1;
}

bool isDuplicateInArray(Fibo &fiboArray[]){
   for (int index = 0; index < ArraySize(fiboArray); index++){
      for (int nextIndex = index + 1; nextIndex < ArraySize(fiboArray); nextIndex++){
         if(isDuplicate(fiboArray[index], fiboArray[nextIndex])){
            setDefaultFibo(fiboArray[index]);
            return true;
         }
      }
   }
   return false;
}

void setDefaultFibo(Fibo &fibo){
   fibo.startPoint = 0;
   fibo.startIndex = 0;
   fibo.endPoint = 0;
   fibo.endIndex = 0;
   fibo.trend = "";
   fibo.priceExceeds500 = false;
   fibo.priceExceeds500Index = 0;
   fibo.priceExceeds236 = false;
   fibo.priceH1Exceeds382 = false;
   fibo.priceExceedsEma = false;
   fibo.order.stopLoss = 0;
}

void setDefaultStatusFibo(Fibo &fibo){
   fibo.priceExceeds500 = false;
   fibo.priceExceeds500Index = 0;
   fibo.priceExceeds236 = false;
   fibo.priceH1Exceeds382 = false;
   fibo.priceExceedsEma = false;
   fibo.order.stopLoss = 0;
   fibo.order.takeProfit = 0;
}

bool isDuplicate(const Fibo &a, const Fibo &b){
   // Kiểm tra các điều kiện dễ tính toán hoặc có khả năng loại bỏ sớm nhất trước
   if (a.startPoint == 0.0 || a.endPoint == 0.0) return false;
   // Kiểm tra các điều kiện còn lại
   return a.startPoint == b.startPoint && a.endPoint == b.endPoint;
}

void addOrderToArray(ulong ticket, Fibo &fibo, string comment){
   int resultIndex = -1;
   for(int index = 0; index < 5; index++){
      if((MyTicketOrder[index].endPoint == fibo.endPoint) 
         && (MyTicketOrder[index].startPoint == fibo.startPoint)){
         resultIndex = index;
      }
   }

   if(resultIndex == -1){
      int nextEmptyIndex = getNextIndexEmptyOrderArray();
      if(nextEmptyIndex != -1){
         MyTicketOrder[nextEmptyIndex].ticket = ticket;
         if(comment == "Fibo500") MyTicketOrder[nextEmptyIndex].numericalOrderFibo500 = 1;
         else if(comment == "Fibo236") MyTicketOrder[nextEmptyIndex].numericalOrderFibo236 = 1;
         MyTicketOrder[nextEmptyIndex].startPoint = fibo.startPoint;
         MyTicketOrder[nextEmptyIndex].endPoint = fibo.endPoint;
      }
   } else {
      MyTicketOrder[resultIndex].ticket = ticket;
   }
}

void addFiboToArray(Fibo &newFibo, Fibo &fiboArray[]){
   if (newFibo.startPoint == 0 || newFibo.endPoint == 0) return;
   
   for(int index = 0; index < ArraySize(fiboArray); index++){
      if(isDuplicate(fiboArray[index], newFibo)){
         return; // Trả về ngay nếu phát hiện trùng lặp
      }
   }
   
   int nextEmptyIndex = getNextIndexEmptyFiboArray(fiboArray);
   if(nextEmptyIndex != -1){
      fiboArray[nextEmptyIndex] = newFibo;
   }
}

int getNextIndexEmptyFiboArray(const Fibo &array[]){
   for(int index = 0; index < ArraySize(array); index++){
      if(array[index].startPoint == 0.0 && array[index].endPoint == 0.0)
         return index;
   }

   return -1;
}

int getNextIndexEmptyOrderArray(){
   for(int index = 0; index < 5; index++){
      if(MyTicketOrder[index].startPoint == 0.0 && MyTicketOrder[index].endPoint == 0.0)
         return index;
   }

   return -1;
}

void drawFiboInChart(Fibo &fiboArray[]){
   // Xóa tất cả các đường xu hướng
   ObjectsDeleteAll(0, -1, OBJ_TREND);
   
   int size = ArraySize(fiboArray);
   datetime startTime = iTime(NULL, 0, 0);
   datetime endTime = iTime(NULL, 0, TWENTY_CANDLES);
   
   for (int index = 0; index < size; index++){
      double priceStart = fiboArray[index].startPoint;
      double priceEnd = fiboArray[index].endPoint;
      double price236 = getFiboData(fiboArray[index].startPoint,fiboArray[index].endPoint, FIBO_236);
      double price500 = getFiboData(fiboArray[index].startPoint,fiboArray[index].endPoint, FIBO_500);
      
      if (priceStart == 0 || priceEnd == 0) continue;

      string nameStart = "Start Fibo " + fiboArray[index].trend + " " + IntegerToString(index);
      string nameEnd = "End Fibo " + IntegerToString(index);
      string name236 = "Fibo 236 " + IntegerToString(index);
      string name500 = "Fibo 500 " + IntegerToString(index);
      
      // Tạo và thiết lập thuộc tính của đường xu hướng bắt đầu
      ObjectCreate(0, nameStart, OBJ_TREND, 0, startTime, priceStart, endTime, priceStart);
      ObjectSetInteger(0, nameStart, OBJPROP_COLOR, clrGreen);
      ObjectSetInteger(0, nameStart, OBJPROP_WIDTH, 2);

      // Tạo và thiết lập thuộc tính của đường xu hướng kết thúc
      ObjectCreate(0, nameEnd, OBJ_TREND, 0, startTime, priceEnd, endTime, priceEnd);
      ObjectSetInteger(0, nameEnd, OBJPROP_COLOR, clrRed);
      ObjectSetInteger(0, nameEnd, OBJPROP_WIDTH, 2);
      
      // Tạo và thiết lập thuộc tính của đường xu hướng 236
      ObjectCreate(0, name236, OBJ_TREND, 0, startTime, price236, endTime, price236);
      ObjectSetInteger(0, name236, OBJPROP_COLOR, clrBlue);
      ObjectSetInteger(0, name236, OBJPROP_WIDTH, 1);
      
      // Tạo và thiết lập thuộc tính của đường xu hướng 500
      ObjectCreate(0, name500, OBJ_TREND, 0, startTime, price500, endTime, price500);
      ObjectSetInteger(0, name500, OBJPROP_COLOR, clrBlue);
      ObjectSetInteger(0, name500, OBJPROP_WIDTH, 1);
   }
}

void drawStruct(){
   ObjectsDeleteAll(0, -1, OBJ_TEXT);
   
   StructPoint pointArray[];
   setStructPointArray(0, pointArray, CANDLES_M15_30_DAYS, _Period);
   
   for(int index = 0; index < ArraySize(pointArray); index++){
      string name = DoubleToString(pointArray[index].point);
      datetime time = iTime(_Symbol, _Period, pointArray[index].index);
      
      if(pointArray[index].type == SPH || pointArray[index].type == LPH){
         ObjectCreate(0, name, OBJ_TEXT, 0, time, pointArray[index].point);
         string text = pointArray[index].type == SPH ? "SPH" : "LPH";
         ObjectSetString(0, name, OBJPROP_TEXT, text);
         color colorText = pointArray[index].type == SPH ? clrCyan : clrGreen;
         ObjectSetInteger(0, name, OBJPROP_COLOR, colorText);
         ObjectSetInteger(0, name, OBJPROP_ANCHOR, ANCHOR_LOWER);
         int sizeText = pointArray[index].type == SPH ? 6 : 10;
         ObjectSetInteger(0, name, OBJPROP_FONTSIZE, sizeText);
      } 
      if(pointArray[index].type == SPL || pointArray[index].type == LPL){
         ObjectCreate(0, name, OBJ_TEXT, 0, time, pointArray[index].point);
         string text = pointArray[index].type == SPL ? "SPL" : "LPL";
         ObjectSetString(0, name, OBJPROP_TEXT, text);
         color colorText = pointArray[index].type == SPL ? clrTomato : clrRed;
         ObjectSetInteger(0, name, OBJPROP_COLOR, colorText);
         ObjectSetInteger(0, name, OBJPROP_ANCHOR, ANCHOR_UPPER);
         int sizeText = pointArray[index].type == SPL ? 6 : 10;
         ObjectSetInteger(0, name, OBJPROP_FONTSIZE, sizeText);
      }  
   }
}  

double getStopLossM15(Fibo &fibo, int shift, int markerFibo) {
   double priFibo = getFiboData(fibo.startPoint, fibo.endPoint, markerFibo);
   double AdditionalStopLoss = getStructField<double>(ADDITIONALSTOPLOSS);
   // Lấy giá trị EMA một lần
   int handleM15 = iMA(_Symbol, PERIOD_M15, PERIOD_EMA, 0, MODE_EMA, PRICE_CLOSE);
   double emaM15[];
   if(handleM15 >= 0) {
      ArraySetAsSeries(emaM15, true);
      CopyBuffer(handleM15, 0, shift, 1, emaM15);
   }

   if (fibo.trend == BUY) {
      // Lấy giá thấp nhất trong hai nến gần nhất
      double stopLoss = MathMin(iLow(_Symbol, PERIOD_M15, shift), iLow(_Symbol, PERIOD_M15, shift + 1));
      stopLoss = MathMin(stopLoss, priFibo); // So sánh với giá trị Fibonacci
      // Đảm bảo Stop Loss nằm dưới giá trị EMA
      if (ArraySize(emaM15) > 0) stopLoss = MathMin(stopLoss, emaM15[0]);
      // Trừ AdditionalStopLoss và chuẩn hóa giá trị
      return NormalizeDouble(stopLoss - AdditionalStopLoss * _Point, _Digits);
   } 
   else if (fibo.trend == SELL) {
      // Lấy giá cao nhất trong hai nến gần nhất
      double stopLoss = MathMax(iHigh(_Symbol, PERIOD_M15, shift), iHigh(_Symbol, PERIOD_M15, shift + 1));
      stopLoss = MathMax(stopLoss, priFibo); // So sánh với giá trị Fibonacci
      // Đảm bảo Stop Loss nằm trên giá trị EMA
      if (ArraySize(emaM15) > 0) stopLoss = MathMax(stopLoss, emaM15[0]);
      // Cộng AdditionalStopLoss và chuẩn hóa giá trị
      return NormalizeDouble(stopLoss + AdditionalStopLoss * _Point, _Digits);
   }

   return 0; // Trả về 0 nếu không có điều kiện nào phù hợp
}

bool isTrendCandle(MqlRates &candle, MqlRates &pre_candle, Fibo &fibo, int markerFibo, int index){
   // Tính toán các giá trị cần thiết
   double bodySize = MathAbs(candle.high - candle.low);
   double upperWickSize = candle.high - MathMax(candle.open, candle.close);
   double lowerWickSize = MathMin(candle.open, candle.close) - candle.low;
   // Kiểm tra nến thuận xu hướng
   bool validTrending = (fibo.trend == BUY && candle.close > candle.open)
                        || (fibo.trend == SELL && candle.close < candle.open);

   if(!validTrending){
      if(fibo.trend == BUY && candle.low < pre_candle.low){
         if(lowerWickSize > bodySize * 4 && upperWickSize < bodySize)
            return true;
      } else if(fibo.trend == SELL && candle.high > pre_candle.high){
         if(upperWickSize > bodySize * 4 && lowerWickSize < bodySize)
            return true;
      }
      return false; // Nếu không thuận xu hướng, trả về false ngay lập tức
   } else {
      // Kiểm tra với cây nến trước đó
      double diffPrice = 10 * _Point;
      if(StringFind(_Symbol, "XAUUSD") > -1) diffPrice = 100 *_Point;
      if(MathAbs(candle.open - pre_candle.close) > diffPrice) return false;

      double fibo500 = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_500);
      double fibo500PriceCandle = getFiboData(pre_candle.low, pre_candle.high, FIBO_500);
      double fibo500ToEndFibo = MathAbs(fibo500 - fibo.endPoint);
      double isCheckCrossCandle = getStructField<double>(ISCHECKCROSSCANDLE);
      if(fibo.trend == BUY){
         if(upperWickSize > (1.0 / 3.0) * bodySize 
            || upperWickSize >= (candle.close - candle.open) * 0.75)
            return false;// Kiểm tra râu nến không vượt quá 1/3 thân nến

         if(pre_candle.close < pre_candle.open && candle.close < fibo500PriceCandle) return false;

         if(pre_candle.close < pre_candle.open ){
            if(candle.low > pre_candle.low
               && candle.high < getFiboData(pre_candle.low, pre_candle.high, FIBO_618))
               return false;
         } else if(isCheckCrossCandle && candle.high <= pre_candle.high){
            return false;
         }
  
         double closeHigh = getPriceHighest(fibo.startIndex, index, CLOSE, false);
         if(candle.high > closeHigh && candle.close <= closeHigh)
            return false;// Kiểm tra giá về vùng đỉnh
         
         double priceLow = getPriceLowest(fibo.endIndex, index, LOW, false);
         double fibo500ToPriceLow = MathAbs(fibo500 - priceLow);
         if(fibo500ToPriceLow < fibo500ToEndFibo * 0.2 && markerFibo == FIBO_236) 
            if(fibo.startIndex > 450) return false;//Kiểm tra điểm pullback có về gần fibo500 không
      } else if(fibo.trend == SELL){
         if(lowerWickSize > (1.0 / 3.0) * bodySize
            || lowerWickSize >= (candle.open - candle.close) * 0.75)
            return false;// Kiểm tra râu nến không vượt quá 1/3 thân nến

         if(pre_candle.close > pre_candle.open && candle.close > fibo500PriceCandle) return false;

         if(pre_candle.close > pre_candle.open){
            if(candle.high < pre_candle.high
               && candle.low > getFiboData(pre_candle.high, pre_candle.low, FIBO_618))
               return false;
         } else if(isCheckCrossCandle && candle.low >= pre_candle.low){
            return false;
         }

         double closeLow = getPriceLowest(fibo.startIndex, index, CLOSE, false);
         if(candle.low < closeLow && candle.close >= closeLow)
            return false;// Kiểm tra giá về vùng đáy

         double priceHigh = getPriceHighest(fibo.endIndex, index, HIGH, false);
         double fibo500ToPriceHigh = MathAbs(fibo500 - priceHigh);
         if(fibo500ToPriceHigh < fibo500ToEndFibo * 0.2 && markerFibo == FIBO_236) 
            if(fibo.startIndex > 450) return false;//Kiểm tra điểm pullback có về gần fibo500 không
      }
   
      // Kiểm tra giá và fibo
      double fiboValue = getFiboData(fibo.startPoint, fibo.endPoint, markerFibo);
      bool validFiboPri = (fibo.trend == BUY) ? (candle.close > fiboValue) : (candle.close < fiboValue);
      
      if(!validFiboPri && markerFibo) return false;
      
      return true;
   }
}

bool isEmaConditions(datetime targetTime, string typeTrend){
   // Kiểm tra shiftM15, shiftH1 và shiftH4
   int shiftM15 = iBarShift(_Symbol, PERIOD_M15, targetTime);
   int shiftH1 = iBarShift(_Symbol, PERIOD_H1, targetTime);
   int shiftH4 = iBarShift(_Symbol, PERIOD_H4, targetTime);
   if (shiftM15 < 0 || shiftH1 < 0 || shiftH4 < 0) return false;
   // Lấy giá đóng cửa tại thời điểm targetTime
   double price = iClose(_Symbol, PERIOD_M15, shiftM15);
   double paddingEma = getStructField<double>(PADDINGEMA);
   double emaM15[], emaH1[], emaH4[];
   //Check EMAM15
   int handleM15 = iMA(_Symbol, PERIOD_M15, PERIOD_EMA, 0, MODE_EMA, PRICE_CLOSE);
   if (handleM15 < 0) return false;
   ArraySetAsSeries(emaM15, true);
   if(CopyBuffer(handleM15, 0, shiftM15, 1, emaM15) > 0){
      if((typeTrend == BUY && price < emaM15[0] + paddingEma * _Point)
         || (typeTrend == SELL && price > emaM15[0] - paddingEma * _Point)){
         return false;
      }
   }
   //Check EMAH1
   int handleH1 = iMA(_Symbol, PERIOD_H1, PERIOD_EMA, 0, MODE_EMA, PRICE_CLOSE);
   if (handleH1 < 0) return false;
   ArraySetAsSeries(emaH1, true);
   if(CopyBuffer(handleH1, 0, shiftH1, 1, emaH1) > 0){
      if((typeTrend == BUY && price < emaH1[0] + paddingEma * _Point)
         || (typeTrend == SELL && price > emaH1[0] - paddingEma * _Point)){
         return false;
      }
   }

   if((typeTrend == BUY && emaH1[0] > emaM15[0]) ||
      (typeTrend == SELL && emaH1[0] < emaM15[0])) return false;
   return true;
}

bool isFiboCondition(Fibo &fibo){
   double fibo236Price = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_236);
   double distance236To0 = MathAbs(fibo236Price - fibo.endPoint)/ _Point;
   double min236Fibo = getStructField<double>(MIN236FIBO);

   return distance236To0 > min236Fibo;
}

bool isConditions(StructPoint &structPointArray[], MqlRates &rates[], Fibo &fibo, int index, int markerFibo){
   datetime timeM15 = rates[index].time;
   double fibo236Price = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_236);
   double entryToEndFibo = MathAbs(rates[index].close - fibo.endPoint);
   double fibo236ToEndFibo = MathAbs(fibo236Price - fibo.endPoint);
   // Kiểm tra thỏa mãn setup trade ngược trend không
   string currentTrend = getMainTrend(timeM15);
   if(currentTrend != fibo.trend)
      if(!isEmaH4PreConditions(timeM15, fibo.trend)) 
         return false;
   // Kiểm tra độ dài của stoploss
   double stopLoss = getStopLossM15(fibo, index, markerFibo);
   double stopLossPoint = MathAbs(rates[index].close - stopLoss)/_Point;
   double minStopLoss = getStructField<double>(MINSTOPLOSS);
   double maxStopLoss = getStructField<double>(MAXSTOPLOSS);

   if(stopLossPoint < minStopLoss || stopLossPoint > maxStopLoss) return false;
   //Kiểm tra điều kiện ema
   if(!isEmaConditions(timeM15, fibo.trend)) return false;
   // Kiểm tra H1
   if(!isEmaPreConditions(fibo.trend, CANDLES_CHECK_EMA_H1, PERIOD_H1)){
      double closeH1End = iClose(_Symbol, PERIOD_H1, TWO);
      double closeH1Start = iClose(_Symbol, PERIOD_H1, CANDLES_CHECK_EMA_H1);
      if(MathAbs(closeH1Start - closeH1End) >= 0.1 * fibo236ToEndFibo)
         return false;
   }
   //Kiểm tra hình thái nến
   if(!isTrendCandle(rates[index], rates[index + 1], fibo, markerFibo, index)) return false;
   // Kiểm tra độ dốc của pullback
   if(!isCheckMomentum(rates, fibo, index, markerFibo)) return false;
   // Kiểm tra xem có cây đảo chiều mạnh không
   if(!isStrongReversalCandle(rates, fibo) && fibo.endIndex - index < 70) return false;
   // Kiểm tra gap
   if(!isGapInRange(fibo.endIndex, index, rates)) return false;
   // Kiểm tra xem nếu có setup 500 thì giá có quá gần fibo 236 không
   double isCheckSetup500 = getStructField<double>(ISCHECKSETUP500);
   if(markerFibo == FIBO_500 && entryToEndFibo > fibo236ToEndFibo && isCheckSetup500){
      if(MathAbs(rates[index].close - fibo236Price) < 0.5 * stopLossPoint * _Point) return false;
   }

   if(markerFibo == FIBO_236){
      //Kiểm tra m15
      if(!isEmaPreConditions(fibo.trend, CANDLES_CHECK_EMA_M15, PERIOD_M15))
         if(!isMazuCandle(rates[ONE], fibo.trend) || !isMazuCandle(rates[TWO], fibo.trend)) return false;
      // Kiểm tra bộ nến đảo chiều
      if(!isReversalPatterns(rates, fibo.trend, fibo.endIndex) && fibo.endIndex - index < 70){
         int handleM15 = iMA(_Symbol, PERIOD_M15, PERIOD_EMA, 0, MODE_EMA, PRICE_CLOSE);
         if (handleM15 < 0) return false;
         double emaM15[];
         ArraySetAsSeries(emaM15, true);
         if(CopyBuffer(handleM15, ZERO, ZERO, fibo.endIndex + ONE, emaM15) > ZERO){
            for(int index = ONE; index <= fibo.endIndex; index++){
               if(((emaM15[index] > rates[index].close && emaM15[index] > rates[index].open)
                  || (rates[index].close < fibo236Price && rates[index].open < fibo236Price))
                  && fibo.trend == BUY) return false;

               if(((emaM15[index] < rates[index].close && emaM15[index] < rates[index].open)
                  || (rates[index].close > fibo236Price && rates[index].open > fibo236Price))
                  && fibo.trend == SELL) return false;    
            }
         }
      }
      // Kiểm tra xem có gần đỉnh Fibo quá không
      if(entryToEndFibo < fibo236ToEndFibo * 0.15
         && ((fibo.trend == SELL && rates[index].close > fibo.endPoint)
         || (fibo.trend == BUY && rates[index].close < fibo.endPoint)))
         return false;
      // Kiểm tra đỉnh là NoSD
      if(!isNosdCandle(rates, fibo.trend, fibo.endIndex)){
         double noSDBody = MathAbs(rates[fibo.endIndex].high - rates[fibo.endIndex].low);
         if(noSDBody >= fibo236ToEndFibo * 0.2) return false;
      }
      // Kiểm tra H1 đảo chiều
      if(!isH1Reverse(fibo, rates)) return false;
   }
   // Kiểm tra cấu trúc thị trường
   if(!isCheckSwingPoint(rates, fibo, structPointArray, index, markerFibo)) return false;
   // Kiểm tra giá sideway
   if(!isSideWay(structPointArray, fibo, rates)) return false;
   // Kiểm tra cây nến end fibo là cấy nến rút râu mạnh
   if(!isLongWickCandle(rates, fibo, fibo.endIndex)) return false;
   // Kiểm tra có dài quá không
   if(!isSoFar(fibo, rates, stopLossPoint)) return false;
   // Kiểm tra các setup break fibo
   if(!isSetupBreakFibo(fibo, rates)) return false;
   return true; 
}

bool isCheckSwingPoint(MqlRates &rates[], Fibo &fibo, StructPoint &structPointArray[], int index, int markerFibo){
   StructPoint LPHPoint = getStructPoint(structPointArray, LPH);
   StructPoint LPLPoint = getStructPoint(structPointArray, LPL);
   StructPoint SPHPoint = getStructPoint(structPointArray, SPH);
   StructPoint SPLPoint = getStructPoint(structPointArray, SPL);
      
   if(fibo.trend == BUY){
      double LPHAverage = calculateAverage(structPointArray, LPH, fibo.startIndex);
      double stopLoss = getStopLossM15(fibo, index, markerFibo);
      double entryToStopLoss = MathAbs(rates[index].close - stopLoss);
      if(LPHAverage != 0.0 && markerFibo == FIBO_236){
         if(rates[index].close - LPHAverage < entryToStopLoss)
            return false;
      }
      
      double priceLowest = getPriceLowest(fibo.endIndex, index, LOW, false);
      int lowestIndex = (int)getPriceLowest(fibo.endIndex, index, LOW, true);
      if(LPHPoint.index == fibo.endIndex){
         if(LPLPoint.index < fibo.endIndex)
            return true;

         if(rates[index].close > SPHPoint.point && SPHPoint.index < fibo.endIndex)
            return true;
         
         if(rates[index].close >= getFiboData(priceLowest, rates[fibo.endIndex].close, FIBO_500))
            return true;
         
         double bodySize = MathAbs(rates[lowestIndex].high - rates[lowestIndex].low);
         double lowerWick = MathMin(rates[lowestIndex].open, rates[lowestIndex].close) - rates[lowestIndex].low;
         if(lowerWick >= (8.5 / 10.0) * bodySize) 
            return true;

      } else if(LPHPoint.index < fibo.endIndex){
         if(rates[LPHPoint.index].close > rates[index].close){
            if(LPHPoint.point - rates[index].close > 0.5 * entryToStopLoss)
               return true;
            
            if(!isReversalPatterns(rates, SELL, lowestIndex)
               || !isNosdCandle(rates, SELL, lowestIndex)
               || !isLongWickCandle(rates, fibo, lowestIndex)){
               return true;
            }
         } else return true;
      } else if(LPHPoint.index > fibo.endIndex){
         return true;
      }
   } else if(fibo.trend == SELL){
      double stopLoss = getStopLossM15(fibo, index, markerFibo);
      double entryToStopLoss = MathAbs(rates[index].close - stopLoss);
      double LPLAverage = calculateAverage(structPointArray, LPL, fibo.startIndex);
      if(LPLAverage != 0.0 && markerFibo == FIBO_236){
         if(LPLAverage - rates[index].close < entryToStopLoss)
            return false;
      }
      
      double priceHighest = getPriceHighest(fibo.endIndex, index, HIGH, false);
      int highestIndex = (int)getPriceHighest(fibo.endIndex, index, HIGH, true);
      if(LPLPoint.index == fibo.endIndex){
         if(LPHPoint.index < fibo.endIndex)
            return true;

         if(rates[index].close < SPLPoint.point && SPLPoint.index < fibo.endIndex)
            return true;
         
         if(rates[index].close <= getFiboData(priceHighest, rates[fibo.endIndex].close, FIBO_500))
            return true;
         
         double bodySize = MathAbs(rates[highestIndex].high - rates[highestIndex].low);
         double upperWick = rates[highestIndex].high - MathMax(rates[highestIndex].open, rates[highestIndex].close);
         if(upperWick >= (8.5 / 10.0) * bodySize)
            return true;

      } else if(LPLPoint.index < fibo.endIndex){
         if(rates[LPLPoint.index].close < rates[index].close){
            if(rates[index].close - LPLPoint.point > 0.5 * entryToStopLoss)
               return true;
               
            if(!isReversalPatterns(rates, BUY, highestIndex)
               || !isNosdCandle(rates, BUY, highestIndex)
               || !isLongWickCandle(rates, fibo, highestIndex)){
               return true;
            }
         } else return true;
      } else if(LPLPoint.index > fibo.endIndex){
         return true;
      }
   }
   return false; 
}

double getLotSize(double stopLossDistance, double riskAmounts){
   // Lấy thông tin về công cụ giao dịch
   double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
   double tickSize = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
   double point = SymbolInfoDouble(_Symbol, SYMBOL_POINT);
   // Kiểm tra giá trị hợp lệ
   if (tickValue <= ZERO || tickSize <= ZERO || point <= ZERO){
      return 0.0;
   }
   // Tính số pips tương ứng với stopLossDistance
   double stopLossPips = stopLossDistance / point;
   // Tính giá trị pip
   double pipValue = tickValue / (tickSize / point);
   // Tính toán kích thước lô
   double lotSize = riskAmounts / (stopLossPips * pipValue);
   // Đảm bảo rằng kích thước lô nằm trong phạm vi cho phép của sàn giao dịch
   double minLot = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
   double maxLot = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
   lotSize = MathMin(MathMax(lotSize, minLot), maxLot);
   // Làm tròn kích thước lô đến số thập phân phù hợp
   double lotStep = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
   lotSize = MathFloor(lotSize / lotStep) * lotStep;

   return lotSize;
}

void setOrder(StructPoint &structPointArray[], MqlRates &candle, Fibo &fibo, string comment, int ratio, TradeType tradeType){
   double lotSize = 0.0, stopLoss = 0.0, takeProfit = 0.0, risk = 0.0, addOne = 1.0;
   if(StringFind(_Symbol, "XAUUSD") > -1) addOne = 100.0;

   if(tradeType == LONGTRADE){
      stopLoss = fibo.order.stopLoss;
      takeProfit = fibo.order.takeProfit;
      risk = RiskLongTrade;
      double stopLossPoints = fabs(candle.close - stopLoss);
      lotSize = getLotSize(stopLossPoints, risk);
   } else if(tradeType == SHORTTRADE){
      if(fibo.trend == BUY){
         StructPoint SPLPoint = getStructPoint(structPointArray, SPL);
         stopLoss = SPLPoint.point;
         takeProfit = candle.close + (candle.close - stopLoss) * ratio;
      } else if(fibo.trend == SELL){
         StructPoint SPHPoint = getStructPoint(structPointArray, SPH);
         stopLoss = SPHPoint.point;
         takeProfit = candle.close + (candle.close - stopLoss) * ratio;
      }
      risk = RiskShortTrade;
      double stopLossPoints = fabs(candle.close - stopLoss);
      lotSize = getLotSize(stopLossPoints, risk);
   }
   datetime currentTime = TimeCurrent();
   MqlDateTime timeStruct;
   TimeToStruct(currentTime, timeStruct); // Chuyển đổi datetime thành MqlDateTime

   timeStruct.sec = ZERO; // Đặt giây về 0
   datetime timeWithoutSeconds = StructToTime(timeStruct); // Chuyển ngược lại thành datetime
   double diff = getStructField<double>(ADDITIONALSTOPLOSS);
   
   if(fibo.trend == BUY){
      double entry = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
      if(MathAbs(iClose(_Symbol, PERIOD_M15, ONE) - entry) < 1.5 * diff * _Point){
         if(!isOrderWithSameOrder(stopLoss, timeWithoutSeconds, comment)){
            if(!Trade.Buy(lotSize, _Symbol, entry, stopLoss, takeProfit, comment)){
               Print("Error placing Buy Order: ", Trade.ResultRetcode());
            } else {
               ulong ticket = Trade.ResultOrder();        
               while(calculatePotentialLoss(ticket) < ratio * risk){
                  takeProfit += addOne * _Point;
                  modifyStopLoss(ticket, stopLoss, takeProfit);
               }
               if(tradeType == LONGTRADE) addOrderToArray(ticket, fibo, comment);
               Alert("Tham Gia Thị Trường");
            }
         }
      }
   } else if(fibo.trend == SELL){
      double entry = SymbolInfoDouble(_Symbol, SYMBOL_BID);
      if(MathAbs(iClose(_Symbol, PERIOD_M15, ONE) - entry) < 1.5 * diff * _Point){
         if(!isOrderWithSameOrder(stopLoss, timeWithoutSeconds, comment)){
            if (!Trade.Sell(lotSize, _Symbol, entry, stopLoss, takeProfit, comment)){
               Print("Error placing Sell Order: ", Trade.ResultRetcode());
            } else {
               ulong ticket = Trade.ResultOrder();      
               while(calculatePotentialLoss(ticket) < ratio * risk){
                  takeProfit -= addOne * _Point; 
                  modifyStopLoss(ticket, stopLoss, takeProfit);
               }
               
               if(tradeType == LONGTRADE) addOrderToArray(ticket, fibo, comment);
               Alert("Tham Gia Thị Trường");
            }
         }
      }
   }
}

double calculatePotentialLoss(ulong ticket){
   if(PositionSelectByTicket(ticket)){
      double openPrice = PositionGetDouble(POSITION_PRICE_OPEN);
      double takeProfit = PositionGetDouble(POSITION_TP);
      double lotSize = PositionGetDouble(POSITION_VOLUME);
      double tpPoints = MathAbs(openPrice - takeProfit) / _Point;
      // Tính giá trị thua lỗ
      double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
      double tickSize = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
      double pointValue = tickValue / (tickSize / _Point); // Giá trị 1 point
      
      return tpPoints * pointValue * lotSize; 
   }
   return 0.0; // Ticket không hợp lệ
}

void managePositions(){
   // Lặp qua tất cả các vị thế đang mở
   for(int index = PositionsTotal() - 1; index >= ZERO; index--){
      // Lấy thông tin vị thế
      ulong ticket = PositionGetTicket(index);
      if(ticket <= 0) continue;
      // Lấy thông tin chi tiết của vị thế
      double entry = PositionGetDouble(POSITION_PRICE_OPEN);
      double stopLoss = PositionGetDouble(POSITION_SL);
      double takeProfit = PositionGetDouble(POSITION_TP);
      double currentPrice = PositionGetDouble(POSITION_PRICE_CURRENT);
      string comment = PositionGetString(POSITION_COMMENT);
      string symbol = PositionGetString(POSITION_SYMBOL);
      ENUM_POSITION_TYPE type = (ENUM_POSITION_TYPE)PositionGetInteger(POSITION_TYPE);

      if(PositionSelectByTicket(ticket) && symbol == _Symbol){
         double stopLossDistance = MathAbs(entry - stopLoss);
         double takeProfitDistance = MathAbs(entry - takeProfit);
         double paddingEndFibo = getStructField<double>(PADDINGENDFIBO);
         double diff = getStructField<double>(ADDITIONALSTOPLOSS);

         double closeCandleOne = iClose(_Symbol, PERIOD_M15, ONE);
         double openCandleNow = iOpen(_Symbol, PERIOD_M15, ZERO);
         bool isHasGap = MathAbs(closeCandleOne - openCandleNow) > TWO * diff * _Point;

         if(type == POSITION_TYPE_BUY && currentPrice > entry){
            bool isBE = BEPrice && currentPrice >= BEPrice - paddingEndFibo * _Point;
            double currentProfit = currentPrice - entry;
            if((isHasGap || isBE || currentProfit >= stopLossDistance * 1.15)
               && entry > stopLoss){
               modifyStopLoss(ticket, entry + 5 * _Point, takeProfit);
            }
            
            if(takeProfitDistance * 0.9 <= currentProfit && stopLossDistance < 10 * _Point){
               if(takeProfitDistance < 2.5 * stopLossDistance){
                  modifyStopLoss(ticket, entry + takeProfitDistance / 2, takeProfit);
               } else modifyStopLoss(ticket, entry + takeProfitDistance / 3, takeProfit);
            }
         }else if(type == POSITION_TYPE_SELL && currentPrice < entry){
            bool isBE = BEPrice && currentPrice <= BEPrice + paddingEndFibo * _Point;
            double currentProfit = entry - currentPrice;
            if((isHasGap || isBE || currentProfit >= stopLossDistance * 1.15)
               && entry < stopLoss) {
               modifyStopLoss(ticket, entry - 5 * _Point, takeProfit);
            }
            
            if(takeProfitDistance * 0.9 <= currentProfit && stopLossDistance < 10 * _Point){
               if(takeProfitDistance < 2.5 * stopLossDistance){
                  modifyStopLoss(ticket, entry - takeProfitDistance / 2, takeProfit);
               } else modifyStopLoss(ticket, entry - takeProfitDistance / 3, takeProfit);               
            }
         }
      }
   }
}
// Hàm hỗ trợ để điều chỉnh Stop Loss
void modifyStopLoss(ulong ticket, double newStopLoss, double takeProfit){
   newStopLoss = NormalizeDouble(newStopLoss, _Digits);
   if (!Trade.PositionModify(ticket, newStopLoss, takeProfit)){
      Print("Failed to modify position #", ticket, ". Error: ", GetLastError());
   }
}
// Hàm kiểm tra giá trị EMA 25 H4 tại một cây nến bất kỳ và xuất giá trị EMA của cây H4 trước đó
bool isEmaH4PreConditions(datetime candleTime, string trend){
   int emaHandle = iMA(_Symbol, PERIOD_H4, 25, ZERO, MODE_EMA, PRICE_CLOSE);
   if(emaHandle < ZERO) return false;
   
   double emaH4[];
   ArraySetAsSeries(emaH4, true);
   // Tìm vị trí của cây nến trên khung thời gian H4
   int h4Index = iBarShift(_Symbol, PERIOD_H4, candleTime, true);
   if(h4Index < ZERO) return false;
   
   if(CopyBuffer(emaHandle, ZERO, h4Index + 1, 1, emaH4) <= ZERO) return false;
   double previous_close = iClose(_Symbol, PERIOD_H4, h4Index + 1);
   
   if((trend == BUY && emaH4[ZERO] < previous_close) || (trend == SELL && emaH4[ZERO] > previous_close)){
      return true;
   }
   
   return false;
}

void checkSetup(Fibo &fibo, MqlRates &rates[], int ticketOrderIndex){
   double isBeStopLoss = getStructField<double>(ISBESTOPLOSS);
   StructPoint structPointArray[];
   setStructPointArray(ONE, structPointArray, CANDLES_M15_30_DAYS, PERIOD_M15);
   if(fibo.priceExceeds236 && !fibo.priceExceeds500 && fibo.priceExceedsEma && !fibo.priceH1Exceeds382){
      if(isConditions(structPointArray, rates, fibo, ONE, FIBO_236)){
         fibo.order.stopLoss = getStopLossM15(fibo, ONE, FIBO_236);
         int ratio = getTakeProfitRatio(fibo.order.stopLoss, rates[ONE].close);
         double takeProfit = rates[ONE].close + (rates[ONE].close - fibo.order.stopLoss) * ratio;
         fibo.order.takeProfit = NormalizeDouble(takeProfit, _Digits);       
         int countOrder = ZERO;
         if(ticketOrderIndex != -1){
            MyTicketOrder[ticketOrderIndex].numericalOrderFibo236++;
            countOrder = MyTicketOrder[ticketOrderIndex].numericalOrderFibo236;
         }
                  
         if(countOrder <= THREE){
            double stopLossDistance = MathAbs(rates[ONE].close - fibo.order.stopLoss);
            double entryToEndFibo = MathAbs(rates[ONE].close - fibo.endPoint);
            
            if(entryToEndFibo >= 0.55 * stopLossDistance && isBeStopLoss){
               BEPrice = fibo.endPoint;
            } else BEPrice = 0.0;
                        
            setOrder(structPointArray, rates[ONE], fibo, "Fibo236", ratio, LONGTRADE);
         }
      }
   } else if(fibo.priceExceeds500 && fibo.priceExceedsEma){
      if(isConditions(structPointArray, rates, fibo, ONE, FIBO_500) && fibo.endIndex - ONE < 200){
         fibo.order.stopLoss = getStopLossM15(fibo, ONE, FIBO_500);
         int ratio = getTakeProfitRatio(fibo.order.stopLoss, rates[ONE].close);
         double takeProfit = rates[ONE].close + (rates[ONE].close - fibo.order.stopLoss) * ratio;
         fibo.order.takeProfit = NormalizeDouble(takeProfit, _Digits);
         int countOrder = ZERO;
         if(ticketOrderIndex != -1){
            MyTicketOrder[ticketOrderIndex].numericalOrderFibo500++;
            countOrder = MyTicketOrder[ticketOrderIndex].numericalOrderFibo500;
         }
         
         if(countOrder <= THREE){
            double stopLossDistance = MathAbs(rates[ONE].close - fibo.order.stopLoss);
            double entryToEndFibo = MathAbs(rates[ONE].close - fibo.endPoint);
            
            if(entryToEndFibo >= 0.5 * stopLossDistance && isBeStopLoss){
               BEPrice = fibo.endPoint;
            } else BEPrice = 0.0;
            
            setOrder(structPointArray, rates[ONE], fibo, "Fibo500", ratio, LONGTRADE);
         }
      }
   }
}

bool isCheckMomentum(MqlRates &rates[], Fibo &fibo, int index, int markerFibo){
   double priceEndPullback = 0.0;
   double isCheckBreakOut = getStructField<double>(ISCHECKBREAKOUT);
   // Xử lý sớm nếu markerFibo không hợp lệ
   if(markerFibo != FIBO_236 && markerFibo != FIBO_500) return false;

   int threshold = (markerFibo == FIBO_236) ? 14 : 9;
   int fiboMultiplier = (markerFibo == FIBO_236) ? 250 : 200;

   if (MathAbs(index - fibo.endIndex) > threshold){
      if (!isCheckBreakOut) return true;

      priceEndPullback = (fibo.trend == BUY)
         ? getPriceLowest(fibo.endIndex, index, LOW, false)
         : getPriceHighest(fibo.endIndex, index, HIGH, false);

      return (fibo.trend == BUY)
         ? rates[index].close >= getFiboData(fibo.endPoint, priceEndPullback, fiboMultiplier)
         : rates[index].close <= getFiboData(fibo.endPoint, priceEndPullback, fiboMultiplier);
   } else {
      priceEndPullback = (fibo.trend == BUY)
         ? getPriceLowest(fibo.endIndex, index, LOW, false)
         : getPriceHighest(fibo.endIndex, index, HIGH, false);

      return (fibo.trend == BUY)
         ? rates[index].close >= getFiboData(priceEndPullback, fibo.endPoint, FIBO_500)
         : rates[index].close <= getFiboData(priceEndPullback, fibo.endPoint, FIBO_500);
   }

   return false;
}

double getPriceHighest(int start, int end, PriceType priceType, bool isGetIndex){
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   int copied = CopyRates(Symbol(), PERIOD_M15, ZERO, start + 1, rates);
   
   double priceHighest = (priceType == HIGH) ? rates[start].high : rates[start].close;
   int indexResult = start;
   
   for(int index = start; index >= end; index--){ 
      double currentPrice = (priceType == HIGH) ? rates[index].high : rates[index].close;
      if(currentPrice > priceHighest){
         priceHighest = currentPrice;
         indexResult = index;
      }
   }
   
   return isGetIndex ? indexResult : priceHighest;
}

double getPriceLowest(int start, int end, PriceType priceType, bool isGetIndex){
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   int copied = CopyRates(Symbol(), PERIOD_M15, ZERO, start + 1, rates);
   
   double priceLowest = (priceType == LOW) ? rates[start].low : rates[start].close;
   int indexResult = start;
   
   for(int index = start; index >= end; index--){ 
      double currentPrice = (priceType == LOW) ? rates[index].low : rates[index].close;
      if(currentPrice < priceLowest){
         priceLowest = currentPrice;
         indexResult = index;
      }
   }
   
   return isGetIndex ? indexResult : priceLowest;
}

template<typename ValueType>
ValueType getStructField(const SymbolInfFieldName fieldName){
   for (int index = 0; index < ArraySize(symbolInf); index++){
      if(StringFind(_Symbol, symbolInf[index].name) > -1){
         if(fieldName == ISBESTOPLOSS)
            return symbolInf[index].beStopLoss;
         else if(fieldName == ADDITIONALSTOPLOSS)
            return symbolInf[index].additionalStopLoss;
         else if(fieldName == MINSTOPLOSS)
            return symbolInf[index].minStopLoss;
         else if(fieldName == MAXSTOPLOSS)
            return symbolInf[index].maxStopLoss;
         else if(fieldName == MIN236FIBO)
            return symbolInf[index].min236Fibo;
         else if(fieldName == ISCHECKSETUP500)
            return symbolInf[index].isCheckSetup500;
         else if(fieldName == PADDINGEMA)
            return symbolInf[index].paddingEma;
         else if(fieldName == ISCHECKCROSSCANDLE)
            return symbolInf[index].isCheckCrossCandle;
         else if(fieldName == PADDINGENDFIBO)
            return symbolInf[index].paddingEndFibo;
         else if(fieldName == ISCHECKBREAKOUT)
            return symbolInf[index].isCheckBreakOut;
         else return ValueType();
      }
   }
   
   return ValueType();
}

string getMainTrend(datetime targetTime){
   // Lấy chỉ số của cây nến tại thời điểm targetTime
   int shiftH4 = iBarShift(_Symbol, PERIOD_H4, targetTime);
   if (shiftH4 < ZERO) return "";
   // Sao chép dữ liệu 42 cây nến H4 từ shiftH4
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   if(CopyRates(_Symbol, PERIOD_H4, shiftH4, CANDLES_H4_7_DAYS, rates) != CANDLES_H4_7_DAYS){
      return "";
   }
   // Lấy handle của EMA25
   int handleH4 = iMA(_Symbol, PERIOD_H4, PERIOD_EMA, ZERO, MODE_EMA, PRICE_CLOSE);
   if (handleH4 < ZERO) return "";
   // Sao chép giá trị EMA25 cho 42 cây nến
   double emaH4[];
   ArraySetAsSeries(emaH4, true);
   if (CopyBuffer(handleH4, 0, shiftH4, CANDLES_H4_7_DAYS, emaH4) != CANDLES_H4_7_DAYS)
      return "";
   // Đếm số cây nến có giá đóng trên/dưới EMA25
   int buyCount = ZERO, sellCount = ZERO;
   for (int index = ZERO; index < CANDLES_H4_7_DAYS; index++){
      if (rates[index].close > emaH4[index]){
         buyCount++;
      } else {
         sellCount++;
      }
   }
   // Trả về "BUY" hoặc "SELL" dựa trên số lượng
   return buyCount > sellCount ? "BUY" : "SELL";
}

bool isOrderWithSameOrder(double stopLossOrder, datetime time, string comment){
   // Duyệt qua tất cả các vị thế đang mở
   for(int index = ZERO; index < PositionsTotal(); index++){
      ulong ticket = PositionGetTicket(index);
      if (ticket > 0){
         // Lấy thông tin vị thế
         string symbol = PositionGetString(POSITION_SYMBOL);
         double orderSL = PositionGetDouble(POSITION_SL);
         datetime orderTime = (datetime)PositionGetInteger(POSITION_TIME);
         string orderComment = PositionGetString(POSITION_COMMENT);
         MqlDateTime timeStruct;
         TimeToStruct(orderTime, timeStruct); // Chuyển đổi datetime thành MqlDateTime
         
         timeStruct.sec = ZERO; // Đặt giây về 0
         datetime timeWithoutSeconds = StructToTime(timeStruct);
          
         if (symbol == _Symbol)
            // Kiểm tra SL có trùng với vị thế hiện tại không
            if (orderSL == stopLossOrder 
               || (timeWithoutSeconds == time && comment == orderComment))
               return true; // Đã tồn tại vị thế có SL trùng
      }
   }
   return false; // Không có vị thế nào trùng SL
}

bool isNosdCandle(const MqlRates &rates[], string trend, int index){
   int preIndex = index + 1, afterIndex = index - 1;
   MqlRates candle = rates[index], preCandle = rates[preIndex], afterCandle = rates[afterIndex];
   double bodySizeCandle = MathAbs(candle.high - candle.low);
   double bodySizePreCandle = MathAbs(preCandle.high - preCandle.low);
   string trendMazu = (trend == BUY) ? SELL : BUY; 
   if(trend == SELL){
      if(candle.close > candle.open 
         && (preCandle.close < preCandle.open || bodySizeCandle >= bodySizePreCandle *5)  
         && (candle.close > preCandle.open 
            || (isMazuCandle(candle, trendMazu) && isMazuCandle(afterCandle, trendMazu)))
         && candle.high > preCandle.high)
         return false;         
   } else if(trend == BUY){
      if(candle.close < candle.open 
         && (preCandle.close > preCandle.open || bodySizeCandle >= bodySizePreCandle *5) 
         && (candle.close < preCandle.open 
            || (isMazuCandle(candle, trendMazu) && isMazuCandle(afterCandle, trendMazu)))
         && candle.low < preCandle.low)
         return false;
   }
   
   return true;
}

bool isLongWickCandle(MqlRates &rates[], Fibo &fibo, int index){
   double bodySize = MathAbs(rates[index].high - rates[index].low);
   double upperWickSize = rates[index].high - MathMax(rates[index].open, rates[index].close);
   double lowerWickSize = MathMin(rates[index].open, rates[index].close) - rates[index].low;
   double fibo236Price = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_236);
   double fibo236ToEndFibo = MathAbs(fibo236Price - fibo.endPoint);
   
   if(fibo.trend == SELL){
      if(lowerWickSize >= (3.0 / 4.0) * bodySize) return false;
      if(lowerWickSize >= (3.0 / 5.0) * fibo236ToEndFibo)
         if(isBreakPullback(fibo, rates)) return false;
   } else if(fibo.trend == BUY){
      if(upperWickSize >= (3.0 / 4.0) * bodySize) return false;
      if(upperWickSize >= (3.0 / 5.0) * fibo236ToEndFibo)
         if(isBreakPullback(fibo, rates)) return false;
   }

   return true;
}

bool isFollowFiboFun(Fibo &fibo, const MqlRates &rates[], int index, double fibo236){
   if(fibo.trend == SELL && rates[index].low < fibo236){
      if(fibo.priceExceeds500Index != index || rates[index].close < rates[index].open)
        return true;
      
      if(rates[index].close > rates[index].open && rates[index].close < fibo236)
         return true;
         
   } else if(fibo.trend == BUY && rates[index].high > fibo236){
      if(fibo.priceExceeds500Index != index || rates[index].close > rates[index].open)
        return true;
      
      if(rates[index].close < rates[index].open && rates[index].close > fibo236)
         return true;
   }
   
   return false;
}

bool isCheckH1Fibo(Fibo &fibo, datetime time_m15, int markerFibo){
   int h1Index = iBarShift(_Symbol, PERIOD_H1, time_m15, false);
   
   double closePrice = iClose(_Symbol, PERIOD_H1, h1Index + ONE);
   double openPrice = iOpen(_Symbol, PERIOD_H1, h1Index + ONE);
   double closePrePrice = iClose(_Symbol, PERIOD_H1, h1Index + TWO);
   double openPrePrice = iOpen(_Symbol, PERIOD_H1, h1Index + TWO);
   double priceFibo = getFiboData(fibo.startPoint, fibo.endPoint, markerFibo);

   if((fibo.trend == SELL) && (closePrice > priceFibo && closePrePrice > priceFibo))
      return openPrice < closePrice && openPrePrice < closePrePrice;
   
   if((fibo.trend == BUY) && (closePrice < priceFibo && closePrePrice < priceFibo))
      return openPrice > closePrice && openPrePrice > closePrePrice;

   return false;
}

bool isStrongReversalCandle(MqlRates &rates[], Fibo &fibo){
   double fibo236Price = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_236);
   double distance236To0 = MathAbs(fibo236Price - fibo.endPoint);
   int index = fibo.endIndex;

   if(fibo.trend == BUY){
      if(rates[index].open > rates[index].close && rates[index].open > fibo236Price){
         if(rates[index].open - rates[index].close >= (3.0 / 4.0) * distance236To0)
            return false;
      }

      double upperWickSize = rates[index].high - MathMax(rates[index].open, rates[index].close);
      if(upperWickSize  >= (7.0 / 10.0) * distance236To0)
         return false;
      
      if(upperWickSize  >= (3.0 / 10.0) * distance236To0 && rates[index].open < fibo236Price)
         return false;
         
   } else if(fibo.trend == SELL){
      if(rates[index].open < rates[index].close && rates[index].open < fibo236Price){
         if(rates[index].close - rates[index].open >= (3.0 / 4.0) * distance236To0)
            return false;
      }

      double lowerWickSize = MathMin(rates[index].open, rates[index].close) - rates[index].low;
      if(lowerWickSize  >= (7.0 / 10.0) * distance236To0)
         return false;
      
      if(lowerWickSize  >= (3.0 / 10.0) * distance236To0 && rates[index].open > fibo236Price)
         return false;
   }

   return true;
}

bool isReversalPatterns(MqlRates &rates[], string trend, int index){
   if(index < THREE) return true;
   // Lấy chỉ số của cây nến trước đó và sau đó
   int preIndex = index + 1, afterIndex = index - 1;
   MqlRates candle = rates[index], preCandle = rates[preIndex], afterCandle = rates[afterIndex];
   
   double upperWickAfterCandle = afterCandle.high - MathMax(afterCandle.open, afterCandle.close);
   double lowerWickAfterCandle = MathMin(afterCandle.open, afterCandle.close) - afterCandle.low;
   //Kiểm tra cây nến doji
   double upperWickCandle = candle.high - MathMax(candle.open, candle.close);
   double lowerWickCandle = MathMin(candle.open, candle.close) - candle.low;
   double bodySize = MathAbs(candle.close - candle.open);
   double candleSize = MathAbs(candle.high - candle.low);
   bool isDoji =  lowerWickCandle > bodySize && upperWickCandle > bodySize && bodySize < (1.0 / 10.0) * candleSize;
   if(trend == SELL){
      double price500PreCandle = getFiboData(preCandle.high, preCandle.low, FIBO_500);
      bool candleValid = candle.close > candle.open;
      if((candleValid || isDoji) 
         && preCandle.close < preCandle.open
         && afterCandle.close > price500PreCandle)
         return false;
      // Ngay sau đỉnh là một cây nhấn chìm
      if(candle.close < candle.open
         && afterCandle.close > afterCandle.open 
         && afterCandle.close >= candle.high
         && upperWickAfterCandle < (1.5 / 10.0) * (afterCandle.close - afterCandle.open))
         return false;
   } else if(trend == BUY){
      double price500PreCandle = getFiboData(preCandle.low, preCandle.high, FIBO_500);
      bool candleValid = candle.close < candle.open;
      if((candleValid || isDoji) 
         && preCandle.close > preCandle.open
         && afterCandle.close < price500PreCandle)
         return false;
      // Ngay sau đỉnh là một cây nhấn chìm
      if(candle.close > candle.open
         && afterCandle.close < afterCandle.open 
         && afterCandle.close <= candle.low
         && lowerWickAfterCandle < (1.5 / 10.0) * (afterCandle.open - afterCandle.close))
         return false;
   }
   return true;
}

bool isGapInRange(int start, int end, const MqlRates &rates[]){
   if(start <= end || end == ZERO) return true;
   bool isHasGapBuy = false, isHasGapSell = false;
   double gapBuyPoint = 0.0, gapSellPoint = 0.0; 
   for (int index = start; index >= end; index--){
      if (rates[index - 1].open > rates[index].close + 15 * _Point && !isHasGapBuy){
         if (rates[index - 1].low > rates[index].high + 15 * _Point){
            isHasGapBuy = true;
            gapBuyPoint = rates[index].high;
            continue;
         }
      }
      
      if(rates[index - 1].open < rates[index].close - 15 * _Point && !isHasGapSell){
         if(rates[index - 1].high < rates[index].low - 15 * _Point){
            isHasGapSell = true;
            gapSellPoint = rates[index].low;
            continue;
         }
      }
      
      if(isHasGapBuy){
         if(rates[index].low <= gapBuyPoint + 5 * _Point){
            isHasGapBuy = false;
            gapBuyPoint = 0.0;
         }
      }
      
      if(isHasGapSell){
         if(rates[index].high >= gapSellPoint - 5 *_Point){
            isHasGapSell = false;
            gapSellPoint = 0.0;
         }
      }
   }
   
   return !isHasGapBuy && !isHasGapSell;
}

void setStructPointArray(int checkIndex, StructPoint &myArray[], int candles, ENUM_TIMEFRAMES tf){
   MqlRates rates[];
   ArraySetAsSeries(rates, true);
   int copied = CopyRates(_Symbol, tf, ZERO, candles, rates);
   if(copied < ZERO) return;
   
   StructPoint structPointArray[];
   ArrayResize(structPointArray, 1000);
   int indexStructPointArray = ZERO;
   
   StructPoint tempSpl, tempSph;
   // Cập nhập cấu trúc
   bool checkLPL = false, checkLPH = false;
   int start = ZERO;
   double temp = 0.0;
   
   int index = ZERO;
   if(getTypeTrend() == BUY){
      tempSpl.index = (int)getPriceLowest(copied - 1, ZERO, LOW, true);
      tempSpl.point = getPriceLowest(copied - 1, ZERO, LOW, false);
      tempSpl.type = SPL;
      tempSph.type = NONE;
      index = tempSpl.index;
      checkLPL = true;
   } else if (getTypeTrend()== SELL){
      tempSph.index = (int)getPriceHighest(copied - 1, ZERO, HIGH, true);
      tempSph.point = getPriceHighest(copied - 1, ZERO, HIGH, false);
      tempSph.type = SPH;
      tempSpl.type = NONE;
      index = tempSph.index;
      checkLPH = true;
   }
   if(index == ZERO) return;

   while(index >= checkIndex){
     // Kiểm tra khi có đáy tạm
      if(tempSpl.type == SPL){
         if(rates[index].low < tempSpl.point){
            // Cập nhật đáy tạm
            tempSpl.index = index;
            tempSpl.point = rates[index].low;
         }  
         
         if(rates[index].close > rates[tempSpl.index].high){
            // Từ đáy tạm trở thành đáy thực
            structPointArray[indexStructPointArray].index = tempSpl.index;
            structPointArray[indexStructPointArray].type = tempSpl.type;
            structPointArray[indexStructPointArray].point = tempSpl.point;
      
            indexStructPointArray++;
            // Khởi tạo đỉnh tạm
            tempSph.index = tempSpl.index;
            tempSph.point = rates[tempSpl.index].high;
            tempSph.type = SPH;
            // Xóa bỏ đáy tạm
            tempSpl.type = NONE;
            
            index = tempSpl.index;
         } 
      } else if(tempSph.type == SPH){
         if(rates[index].high > tempSph.point){
            // Cập nhật đỉnh tạm
            tempSph.index = index;
            tempSph.point = rates[index].high;
         }
         
         if(rates[index].close < rates[tempSph.index].low){
            // Từ đỉnh tạm trở thành đỉnh thực
            structPointArray[indexStructPointArray].index = tempSph.index;
            structPointArray[indexStructPointArray].type = tempSph.type;
            structPointArray[indexStructPointArray].point = tempSph.point;
            indexStructPointArray++;
            // Khởi tạo đáy tạm
            tempSpl.index = tempSph.index;
            tempSpl.point = rates[tempSph.index].low;
            tempSpl.type = SPL;
            // Xóa bỏ đỉnh tạm
            tempSph.type = NONE;
            
            index = tempSph.index;
         } 
      }
      
      index--;
   }
   // Sao chép dữ liệu từ mảng tạm vào mảng kết quả
   ArrayResize(myArray, indexStructPointArray);
   for(int index = ZERO; index < indexStructPointArray; index++){
      myArray[index] = structPointArray[index];
   }
   
   int i = ZERO;
   while(i < indexStructPointArray){
      if(checkLPH && myArray[i].type == SPL){
         if(temp != 0.0){
            if(temp <= myArray[i].point){
               temp = myArray[i].point;
            } else {
               int result = getIndexPointHigh(myArray, start, i);
               myArray[result].type = LPH;
               temp = myArray[result].point;
               checkLPL = true;
               checkLPH = false;
               start = result;
               
               if(i < indexStructPointArray){
                  i = result;
                  continue;
               }
            }
         } else temp = myArray[i].point;
      } else if(checkLPL && myArray[i].type == SPH){
         if(temp != 0.0){
            if(temp >= myArray[i].point){
               temp = myArray[i].point;
            } else {
               int result = getIndexPointLow(myArray, start, i);
               myArray[result].type = LPL;
               
               temp = myArray[result].point;
               checkLPL = false;
               checkLPH = true;
               start = result;
               
               if(i < indexStructPointArray){
                  i = result;
                  continue;
               }
            }
         } else temp = myArray[i].point;
      }
      i++;
   }
}

int getIndexPointHigh(StructPoint &structPointArray[], int start, int end){
   double high = 0.0;
   int result = ZERO;
   for(int index = start + 1; index < end; index++){
      if(structPointArray[index].type == SPH && structPointArray[index].point > high){
         high = structPointArray[index].point;
         result = index;
      }
   }
   
   return result;
}

int getIndexPointLow(StructPoint &structPointArray[], int start, int end){
   double low = structPointArray[start].point;
   int result = ZERO;
   for(int index = start + 1; index < end; index++){
      if(structPointArray[index].type == SPL && structPointArray[index].point < low){
         low = structPointArray[index].point;
         result = index;
      }
   }
   
   return result;
}

StructPoint getStructPoint(StructPoint &structPointArray[], StructType typePoint){
   StructPoint defaultPoint = {ZERO, 0.0, NONE};

   for(int index = ArraySize(structPointArray) - ONE; index >= ZERO; index--){
      if(structPointArray[index].type == typePoint) 
         return structPointArray[index];
   }
   
   return defaultPoint;
}

double calculateAverage(StructPoint &structPointArray[], StructType typePoint, int start){
   double sum = 0.0, count = 0.0;
   for(int index = ArraySize(structPointArray) - ONE; index >= ZERO; index--){
      if(structPointArray[index].index < start && structPointArray[index].type == typePoint){
         sum += structPointArray[index].point;
         count++;
      }
      
      if(structPointArray[index].index >= start) break;
   }
   
   if(count <= 15) return 0.0;
   
   return sum / count;
}

int getTakeProfitRatio(double stopLoss, double entry){
   double stopLossPoint = MathAbs(entry - stopLoss)/_Point;
   double maxStopLoss = getStructField<double>(MAXSTOPLOSS);

   if(stopLossPoint > maxStopLoss * (4.0 / 5.0)) return TWO;
   
   return THREE;
}

bool isSideWay(StructPoint &structPointArray[], Fibo &fibo, MqlRates &rates[]){
   double countResult = ZERO, countCandleFibo = ZERO;
   
   if(fibo.startIndex - ONE < 350) return true;
   
   double fibo236Price = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_236);  
   for(int index = fibo.startIndex; index >= ONE; index--){
      if((rates[index].open > fibo236Price && fibo.trend == BUY)
         || (rates[index].open < fibo236Price && fibo.trend == SELL)){
         countResult++;
      }
      countCandleFibo++;
   }
   
   StructPoint LPHPoint = getStructPoint(structPointArray, LPH);
   StructPoint LPLPoint = getStructPoint(structPointArray, LPL);
   // Nếu số cây ở cùng phía Fibo236 thì kiểm tra vượt đỉnh đảo chiều gần nhất
   if(countResult/countCandleFibo > 0.4){
      if((fibo.trend == BUY && rates[ONE].close < LPHPoint.point)
         || (fibo.trend == SELL && rates[ONE].close > LPLPoint.point))
         return false;
   }
   return true;
}

bool isH1Reverse(Fibo &fibo, MqlRates &rates[]){
   int h1Index = iBarShift(_Symbol, PERIOD_H1, rates[fibo.endIndex].time, false);
   if(h1Index == ZERO) return true;
   
   MqlRates ratesH1[];
   ArraySetAsSeries(ratesH1, true);
   if(CopyRates(_Symbol, PERIOD_H1, ZERO, h1Index + 5, ratesH1) <= ZERO) return true;
   
   double diffPrice = 10 * _Point;
   if(StringFind(_Symbol, "XAUUSD") > -1) diffPrice = 100 *_Point;
   if(MathAbs(ratesH1[h1Index].open - ratesH1[h1Index + 1].close) > diffPrice) return true;

   if(fibo.trend == BUY && ratesH1[h1Index].high == fibo.endPoint){
      if(!isNosdCandle(ratesH1, fibo.trend, h1Index)) return false;
      if(!isReversalPatterns(ratesH1, fibo.trend, h1Index) && h1Index < 20)
         if(isBreakPullback(fibo, rates)) return false;
   } else if(fibo.trend == SELL && ratesH1[h1Index].low == fibo.endPoint){
      if(!isNosdCandle(ratesH1, fibo.trend, h1Index)) return false;
      if(!isReversalPatterns(ratesH1, fibo.trend, h1Index) && h1Index < 20)
         if(isBreakPullback(fibo, rates)) return false;
   }
   
   return true;
}

bool isSoFar(Fibo &fibo, MqlRates &rates[], double stopLossPoint){
   if(fibo.trend == BUY){
      int priceEndPullbackIndex = (int)getPriceLowest(fibo.endIndex, ONE, LOW, true);
      int maxPriceEntryIndex = (int)getPriceHighest(priceEndPullbackIndex, ONE, HIGH, true);
      if(maxPriceEntryIndex > ONE && maxPriceEntryIndex != priceEndPullbackIndex){
         double maxPriceEntry = getPriceHighest(priceEndPullbackIndex, ONE, HIGH, false);
         double entryToEndFibo = MathAbs(maxPriceEntry- fibo.endPoint) / _Point;
         if(entryToEndFibo < 0.25 * stopLossPoint) return false;
      }
   } else if(fibo.trend == SELL){
      int priceEndPullbackIndex = (int)getPriceHighest(fibo.endIndex, ONE, HIGH, true);
      int minPriceEntryIndex = (int)getPriceLowest(priceEndPullbackIndex, ONE, LOW, true);
      if(minPriceEntryIndex > ONE && minPriceEntryIndex != priceEndPullbackIndex){
         double minPriceEntry = getPriceLowest(priceEndPullbackIndex, ONE, LOW, false);
         double entryToEndFibo = MathAbs(minPriceEntry- fibo.endPoint) / _Point;
         if(entryToEndFibo < 0.25 * stopLossPoint) return false;
      }
   }
   
   return true;
}

bool isSetupBreakFibo(Fibo &fibo, MqlRates &rates[]){
   double fibo236Price = getFiboData(fibo.startPoint, fibo.endPoint, FIBO_236);
   if(fibo.trend == BUY && rates[ONE].close > fibo.endPoint){
      int index = ONE;
      while(index < fibo.endIndex){
         if(rates[index].low < fibo236Price && rates[index].close > fibo236Price) 
            break;
         index++;
      }

      if(index > 15) return false;
   } else if(fibo.trend == SELL && rates[ONE].close < fibo.endPoint){
      int index = ONE;
      while(index < fibo.endIndex){
         if(rates[index].high > fibo236Price && rates[index].close < fibo236Price)
            break;
         index++;
      }

      if(index > 15) return false;
   }

   return true;
}

bool isBreakPullback(Fibo &fibo, MqlRates &rates[]){
   if(fibo.trend == BUY && rates[ONE].close < fibo.endPoint){
      double priceEndPullback = getPriceLowest(fibo.endIndex, ONE, LOW, false);
      if(rates[ONE].close < getFiboData(priceEndPullback, fibo.endPoint, FIBO_500))
         return true;
   } else if(fibo.trend == SELL && rates[ONE].close > fibo.endPoint){
      double priceEndPullback = getPriceHighest(fibo.endIndex, ONE, HIGH, false);
      if(rates[ONE].close > getFiboData(priceEndPullback, fibo.endPoint, FIBO_500))
         return true;
   }
   return false;
}

bool isEmaPreConditions(string trend, int candleCheck, ENUM_TIMEFRAMES timeFrame){
   int emaHandle = iMA(_Symbol, timeFrame, PERIOD_EMA, 0, MODE_EMA, PRICE_CLOSE);
   if(emaHandle < ZERO) return true;
   
   double emaArray[];
   ArraySetAsSeries(emaArray, true);
   if(CopyBuffer(emaHandle, 0, ONE, candleCheck + TWO, emaArray) <= ZERO) return true;
   
   for(int index = ZERO; index < candleCheck; index++){
      double ema = emaArray[index + ONE];
      double close = iClose(_Symbol, timeFrame, index + TWO);
      bool isTrendBuy = (trend == BUY);
      bool priceAboveEMA = close > ema;
      // Nếu nến tăng và trên EMA, hoặc nến giảm và dưới EMA thì không ngược chiều
      if((isTrendBuy && priceAboveEMA) || (!isTrendBuy && !priceAboveEMA)) return true;
   }
   // Nếu tất cả các cây nến đều ngược chiều EMA
   return false;
}

bool isMazuCandle(MqlRates &candle, string trend){
   bool isBullish = candle.close > candle.open;
   if((trend == BUY && isBullish) || (trend == SELL && !isBullish)) {
      double bodySize = MathAbs(candle.close - candle.open);
      double candleSize = (trend == BUY) ? (candle.high - candle.open) : (candle.open - candle.low);
      
      return bodySize >= 0.85 * MathAbs(candleSize);
   }
   
   return false;
}

bool fridayTimeIsActive(){
   MqlDateTime dateTime;
   TimeGMT(dateTime);
   if (dateTime.day_of_week != FRIDAY)  return false;
   // Tính endTime chỉ khi cần thiết (sau khi xác định là thứ Sáu)
   bool isDST = (dateTime.mon >= 3 && dateTime.mon <= 10);
   // Giờ đóng cửa Thứ 6: 21:00 GMT (Mùa Đông) hoặc 20:00 GMT (Mùa Hè)
   string InpFridayCloseTimeGMT = isDST ? "20:55" : "20:55";
   datetime endTime = StringToTime(TimeToString(TimeGMT(), TIME_DATE) + " " + InpFridayCloseTimeGMT);

   return (TimeGMT() >= endTime);
}

void closeAllPositions(){
   for(int index = PositionsTotal() - 1; index >= 0 && !IsStopped(); index--){
      ulong ticket = PositionGetTicket(index);
      if(ticket <= 0) continue;
      
      string symbol = PositionGetString(POSITION_SYMBOL);
      if(PositionSelectByTicket(ticket) && symbol == _Symbol){
         double currentProfit = PositionGetDouble(POSITION_PROFIT);
         // Đóng lệnh ngay mà không cần kiểm tra Magic Number
         if(currentProfit < -10){
            if(Trade.PositionClose(ticket)){
               Print("Closed position #", ticket, " - ", symbol);
            } else Print("Close failed #", ticket, " - Error: ", Trade.ResultComment());
         }
      }
   }
}