#include <Trade\Trade.mqh>

CTrade Trade;

datetime CandleCloseTime; 

sinput string separator0 = "------------------------------------------"; // === CÀI ĐẶT BẢN QUYỀN ===
input string Input_LicenseKey = ""; // NHẬP KEY ADMIN CUNG CẤP
sinput string separator1 = "------------------------------------------"; // === QUẢN LÝ RỦI RO ===
input double RiskTrade = 100; // Rủi ro cho mỗi lệnh (USD)
ulong  MagicNumber = 123456; // ID định danh của Bot 

//--- CẤU HÌNH GIAO DỊCH ---
double RiskRewardRatio = 0.5; 
double MinDistanceSL = 2500; 
double MaxDistanceSL = 5000;

double RatioSLDistance = 0.5; 

//--- CẤU HÌNH THỜI GIAN SỬ DỤNG BOT ---
const int DAYS30 = 2592000; 

// --- ENUM LOẠI NẾN ---
enum CandleType {
    Bollinger, 
    Normal 
};

// --- CẤU HÌNH BẢO MẬT ---
string SecretSalt = "20042000";

// --- BIẾN GLOBAL HANDLE CHỈ BÁO ---
int bbHandle = INVALID_HANDLE;

//+------------------------------------------------------------------+
//| Expert initialization function                                   |
//+------------------------------------------------------------------+
int OnInit(){
    // Cài đặt Magic Number cho EA
    Trade.SetExpertMagicNumber(MagicNumber);
    
    // Khởi tạo Handle
    bbHandle  = iBands(_Symbol, PERIOD_M5, 20, 0, 2.0, PRICE_CLOSE);
    
    if(bbHandle == INVALID_HANDLE){
        Print("Lỗi khởi tạo Indicator!");
        return(INIT_FAILED);
    }
    
    EventSetTimer(1);
    return (INIT_SUCCEEDED);
}

void OnDeinit(const int reason){
    EventKillTimer();
    // Giải phóng bộ nhớ chỉ báo
    IndicatorRelease(bbHandle);
}

void OnTimer(){
    datetime currentTime = TimeCurrent();
    datetime currentCandleCloseTime = iTime(_Symbol, PERIOD_M5, 0) + PeriodSeconds(PERIOD_M5);

    if(currentCandleCloseTime != CandleCloseTime && (currentCandleCloseTime - currentTime <= 1)){
        CandleCloseTime = currentCandleCloseTime;
        
        if (!CheckLicense()){
            ExpertRemove();
            return;        
        }

        RunningEA();
    }
}

void OnTick(){
    if(PositionsTotal() > 0){
        TrailingByProfitUSD();
    }
}

void RunningEA(){
    MqlRates rates[];
    ArraySetAsSeries(rates, true);
    if(CopyRates(_Symbol, PERIOD_M5, 0, 3, rates) <= 0) return; // Chỉ cần lấy 3 nến là đủ
    
    Trading(rates);
}

void Trading(const MqlRates &rates[]){
    MqlRates candle = rates[0], secondCandle = rates[1], thirdCandle = rates[2];
    
    double upperShadow = candle.high - MathMax(candle.open, candle.close);
    double lowerShadow = MathMin(candle.open, candle.close) - candle.low;

    if(candle.close > candle.open){
        if(secondCandle.close < secondCandle.open  
            && candle.close > secondCandle.open
            && MathAbs(candle.open - secondCandle.close) <= 100 * _Point
            && candle.high > secondCandle.high
            && candle.low < secondCandle.low
            && candle.low < thirdCandle.low
            && (candle.close - candle.open) * 0.3 > upperShadow
        ){
            BUY(candle, true); 
        } else if(upperShadow <= 0.15 * (candle.close - candle.open)
            && (candle.high > secondCandle.high || candle.low < secondCandle.low)
        ){
            if(!checkBollingerConditions("BUY", candle)){
                BUY(candle, true); 
            } else {
                BUY(candle, false, Bollinger); 
            }
        }
    } else if(candle.close < candle.open){
        if(secondCandle.close > secondCandle.open 
            && candle.close < secondCandle.open
            && MathAbs(candle.open - secondCandle.close) <= 100 * _Point 
            && candle.low < secondCandle.low
            && candle.high > secondCandle.high
            && candle.high > thirdCandle.high
            && (candle.open - candle.close) * 0.3 > lowerShadow
        ){
            SELL(candle, true); 
        } else if(lowerShadow <= 0.15 * (candle.open - candle.close)
            && (candle.high > secondCandle.high || candle.low < secondCandle.low)
        ){
            if(!checkBollingerConditions("SELL", candle)){
                SELL(candle, true); 
            } else {
                SELL(candle, false, Bollinger); 
            }
        }
    }
}

double GetLotSize(double stopLossDistance){
    double tickValue = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_VALUE);
    double tickSize = SymbolInfoDouble(_Symbol, SYMBOL_TRADE_TICK_SIZE);
    if(tickSize == 0 || tickValue == 0) return 0.01;

    double stopLossPips = stopLossDistance / _Point;
    if(stopLossPips == 0) return 0.01;

    double pipValue = tickValue / (tickSize / _Point);
    double lotSize = RiskTrade / (stopLossPips * pipValue);
    
    double minLot = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MIN);
    double maxLot = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_MAX);
    lotSize = MathMin(MathMax(lotSize, minLot), maxLot);
    
    double lotStep = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
    return MathFloor(lotSize / lotStep) * lotStep;
}

void TrailingByProfitUSD(){
    MqlTick last_tick;
    if(!SymbolInfoTick(_Symbol, last_tick)) return;

    double trailingStep = 10 * _Point; 

    for(int index = PositionsTotal() - 1; index >= 0; index--){
        ulong ticket = PositionGetTicket(index);
        if(PositionSelectByTicket(ticket)){
            if(PositionGetString(POSITION_SYMBOL) != _Symbol || PositionGetInteger(POSITION_MAGIC) != MagicNumber) continue;

            double currentSL = PositionGetDouble(POSITION_SL);
            double currentTP = PositionGetDouble(POSITION_TP);
            double priceOpen = PositionGetDouble(POSITION_PRICE_OPEN);
            string comment = PositionGetString(POSITION_COMMENT);
            
            double distanceFromOpen = StringToDouble(comment);
            if(distanceFromOpen <= 0) continue; // Tránh chia 0 hoặc lỗi logic

            if(PositionGetInteger(POSITION_TYPE) == POSITION_TYPE_BUY){
                double profit = last_tick.bid - priceOpen;
                double initialSL = priceOpen - distanceFromOpen;
                
                if(currentTP > 0){
                    double newSL = NormalizeDouble(initialSL + (profit * 1.5), _Digits);
                    if(newSL >= currentSL + trailingStep && newSL < last_tick.bid){
                        Trade.PositionModify(ticket, newSL, currentTP);
                    }
                } else if(profit >= distanceFromOpen){
                    double newSL = NormalizeDouble(initialSL + profit, _Digits);
                    if(newSL >= currentSL + trailingStep && newSL < last_tick.bid) {
                        Trade.PositionModify(ticket, newSL, currentTP);
                    }                    
                }
            } else if(PositionGetInteger(POSITION_TYPE) == POSITION_TYPE_SELL){
                double profit = priceOpen - last_tick.ask;
                double initialSL = priceOpen + distanceFromOpen;
                
                if(currentTP > 0){
                    double newSL = NormalizeDouble(initialSL - (profit * 1.5), _Digits);
                    if(newSL <= currentSL - trailingStep && newSL > last_tick.ask){
                        Trade.PositionModify(ticket, newSL, currentTP);
                    }
                } else if(profit >= distanceFromOpen){
                    double newSL = NormalizeDouble(initialSL - profit, _Digits);
                    if(newSL <= currentSL - trailingStep && newSL > last_tick.ask) {
                        Trade.PositionModify(ticket, newSL, currentTP);
                    }
                }
            }
        }
    }
}

void BUY(MqlRates &candle, bool hasTakeProfit = false, CandleType candleType = Normal){
    double entry = SymbolInfoDouble(_Symbol, SYMBOL_ASK);
    double slDistance = (entry - candle.low);
    if(candleType == Normal) slDistance *= RatioSLDistance;

    double sl = entry - slDistance;
    if(!isOpenOrder(entry, sl)) return;

    double lotSize = GetLotSize(MathAbs(entry - sl));
    
    if(CountPositions("BUY") > 1){
        double lotStep = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
        lotSize = MathFloor((lotSize / lotStep) / 2) * lotStep;
    }

    double takeProfit = hasTakeProfit ? (entry + RiskRewardRatio * MathAbs(entry - sl)) : 0.0;
    
    if(!Trade.Buy(lotSize, _Symbol, entry, sl, takeProfit, DoubleToString(slDistance))){
        Print("Error placing Buy Order: ", Trade.ResultRetcode());
    }
}

void SELL(MqlRates &candle, bool hasTakeProfit = false, CandleType candleType = Normal){
    double entry = SymbolInfoDouble(_Symbol, SYMBOL_BID);
    double slDistance = (candle.high - entry);
    if(candleType == Normal) slDistance *= RatioSLDistance;

    double sl = entry + slDistance;
    if(!isOpenOrder(entry, sl)) return;

    double lotSize = GetLotSize(MathAbs(entry - sl));
    
    if(CountPositions("SELL") > 1){
        double lotStep = SymbolInfoDouble(_Symbol, SYMBOL_VOLUME_STEP);
        lotSize = MathFloor((lotSize / lotStep) / 2) * lotStep;
    }

    double takeProfit = hasTakeProfit ? (entry - RiskRewardRatio * MathAbs(entry - sl)) : 0.0;
    
    if(!Trade.Sell(lotSize, _Symbol, entry, sl, takeProfit, DoubleToString(slDistance))){
        Print("Error placing Sell Order: ", Trade.ResultRetcode());
    }
}

bool isOpenOrder(double entry, double sl){
    return (MathAbs(entry - sl) >= MinDistanceSL * _Point) && (MathAbs(entry - sl) <= MaxDistanceSL * _Point);
}

int CountPositions(string type) {
    int count = 0;
    for(int index = PositionsTotal() - 1; index >= 0; index--){
        ulong ticket = PositionGetTicket(index);
        if(PositionSelectByTicket(ticket)){
            if(PositionGetString(POSITION_SYMBOL) != _Symbol || PositionGetInteger(POSITION_MAGIC) != MagicNumber) continue;

            if(type == "BUY" && PositionGetInteger(POSITION_TYPE) == POSITION_TYPE_BUY) count++;
            else if(type == "SELL" && PositionGetInteger(POSITION_TYPE) == POSITION_TYPE_SELL) count++;
        }
    }
    return count;
}

bool checkBollingerConditions(string trend, MqlRates &candle){
    double upperBand[], lowerBand[], middleBand[];
    ArraySetAsSeries(upperBand, true);
    ArraySetAsSeries(lowerBand, true);
    ArraySetAsSeries(middleBand, true);

    if(CopyBuffer(bbHandle, 1, 0, 1, upperBand) <= 0) return false;
    if(CopyBuffer(bbHandle, 2, 0, 1, lowerBand) <= 0) return false;
    if(CopyBuffer(bbHandle, 0, 0, 1, middleBand) <= 0) return false;

    if(trend == "BUY" && candle.low < lowerBand[0] && candle.high < middleBand[0]) return true;
    if(trend == "SELL" && candle.high > upperBand[0] && candle.low > middleBand[0]) return true;

    return false;
}

string HashEngine(string data) {
    uint hash = 5381;
    for(int i = 0; i < StringLen(data); i++)
        hash = ((hash << 5) + hash) + StringGetCharacter(data, i);
    return StringFormat("%08X", hash);
}

bool CheckLicense() {
    long accID = AccountInfoInteger(ACCOUNT_LOGIN);
    datetime now = TimeCurrent();
    MqlDateTime mqlNow;
    TimeToStruct(now, mqlNow); 

    string timeData = IntegerToString(accID) + SecretSalt + 
                      IntegerToString(mqlNow.mon) + 
                      IntegerToString(mqlNow.year);
    string expectedKey = HashEngine(timeData);

    if (Input_LicenseKey != expectedKey) {
        Alert("Mã Key không đúng! Vui lòng liên hệ Admin. SĐT/Zalo: 0866797299");
        return false;
    }

    if (!GlobalVariableCheck(IntegerToString(accID))) {
        GlobalVariableSet(IntegerToString(accID), (double)now);
        return true;
    }

    datetime activationDate = (datetime)GlobalVariableGet(IntegerToString(accID));
    if(activationDate + DAYS30 < TimeCurrent()) {
        Alert("Thời gian sử dụng bot đã hết hạn! Vui lòng liên hệ SĐT/Zalo: 0866797299");
        GlobalVariableDel(IntegerToString(accID));
        return false;
    }

    return true;
}