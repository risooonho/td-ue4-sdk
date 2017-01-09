// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TreasureDataPrivatePCH.h"
#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Display, All);
IMPLEMENT_MODULE( FAnalyticsTreasureData, TreasureData )

TSharedPtr<IAnalyticsProvider> FAnalyticsProviderTreasureData::Provider;

void FAnalyticsTreasureData::StartupModule()
{
}


void FAnalyticsTreasureData::ShutdownModule()
{
     FAnalyticsProviderTreasureData::Destroy();
}

TSharedPtr<IAnalyticsProvider> FAnalyticsTreasureData::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
    if (GetConfigValue.IsBound()) {
            const FString Key = GetConfigValue.Execute(TEXT("TDApiKey"), true);
            const FString DBName = GetConfigValue.Execute(TEXT("TDDatabase"), true);
            const FString TableName = GetConfigValue.Execute(TEXT("TDTable"), true);

            return FAnalyticsProviderTreasureData::Create(Key, DBName, TableName);
    }
    else {
      UE_LOG(LogAnalytics, Warning,
             TEXT("FAnalyticsTreasureData::CreateAnalyticsProvider called with an unbound delegate"));
    }

    return nullptr;
}

// Provider
FAnalyticsProviderTreasureData::FAnalyticsProviderTreasureData(const FString Key,
                                                               const FString DBName,
                                                               const FString TableName) :
  ApiKey(Key),
  Database(DBName),
  Table(TableName)
{

  //FileArchive = nullptr;
	//AnalyticsFilePath = FPaths::GameSavedDir() + TEXT("Analytics/");
  //	UserId = FPlatformMisc::GetUniqueDeviceId();
}

FAnalyticsProviderTreasureData::~FAnalyticsProviderTreasureData()
{
	if (bHasSessionStarted)
	{
		EndSession();
	}
}

bool FAnalyticsProviderTreasureData::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
        UE_LOG(LogAnalytics, Display, TEXT("[TD] Session Started %s"), *ApiKey);

        if (!bHasSessionStarted && ApiKey.Len() > 0)
	{
             bHasSessionStarted = true;

             SessionId = UserId + TEXT("-") + FDateTime::Now().ToString();
             UE_LOG(LogAnalytics, Display, TEXT("[TD] Session started for user (%s) and session id (%s)"), *UserId, *SessionId);
        }

        if (bHasSessionStarted)
          {
            FString outStr;
            TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&outStr);
            JsonWriter->WriteObjectStart();

            int64 now_unix = FDateTime::Now().ToUnixTimestamp();
            JsonWriter->WriteValue(FString("start_time"), now_unix);
            JsonWriter->WriteObjectEnd();
            JsonWriter->Close();

            TSharedRef< IHttpRequest > HttpRequest = FHttpModule::Get().CreateRequest();
            HttpRequest->SetVerb("POST");
            HttpRequest->SetHeader("Content-Type", "application/json");
            HttpRequest->SetHeader("X-TD-Write-Key", ApiKey);
            HttpRequest->SetURL(FAnalyticsTreasureData::GetAPIURL() + Database + FString("/sessions"));
            HttpRequest->SetContentAsString(outStr);

            HttpRequest->OnProcessRequestComplete().BindRaw(this, &FAnalyticsProviderTreasureData::EventRequestComplete);
            // Execute the request
            HttpRequest->ProcessRequest();
          }

	return bHasSessionStarted;
}

void FAnalyticsProviderTreasureData::EndSession()
{
        if (bHasSessionStarted)
        {
            FString outStr;
            TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&outStr);
            JsonWriter->WriteObjectStart();

            int64 now_unix = FDateTime::Now().ToUnixTimestamp();
            JsonWriter->WriteValue(FString("end_time"), now_unix);
            JsonWriter->WriteObjectEnd();
            JsonWriter->Close();

            TSharedRef< IHttpRequest > HttpRequest = FHttpModule::Get().CreateRequest();
            HttpRequest->SetVerb("POST");
            HttpRequest->SetHeader("Content-Type", "application/json");
            HttpRequest->SetHeader("X-TD-Write-Key", ApiKey);
            HttpRequest->SetURL(FAnalyticsTreasureData::GetAPIURL() + Database + FString("/sessions"));
            HttpRequest->SetContentAsString(outStr);

            HttpRequest->OnProcessRequestComplete().BindRaw(this, &FAnalyticsProviderTreasureData::EventRequestComplete);
            // Execute the request
            HttpRequest->ProcessRequest();

            bHasSessionStarted = false;
            UE_LOG(LogAnalytics, Display, TEXT("[TD] Session Ended"));
        }
}

void FAnalyticsProviderTreasureData::FlushEvents()
{
}


void FAnalyticsProviderTreasureData::SetUserID(const FString& InUserID)
{
  	if (!bHasSessionStarted)
	{
		UserId = InUserID;
		UE_LOG(LogAnalytics, Display, TEXT("[TD] User is now (%s)"), *UserId);
	}
	else
	{
		// Log that we shouldn't switch users during a session
		UE_LOG(LogAnalytics, Warning, TEXT("FAnalyticsProviderTreasureData::SetUserID called while a session is in progress. Ignoring."));
	}
}

FString FAnalyticsProviderTreasureData::GetUserID() const
{
	return UserId;
}


FString FAnalyticsProviderTreasureData::GetSessionID() const
{
	return SessionId;
}

bool FAnalyticsProviderTreasureData::SetSessionID(const FString& InSessionID)
{
        return true;
}

void FAnalyticsProviderTreasureData::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
         if (!bHasSessionStarted)
         {
              UE_LOG(LogAnalytics, Warning, TEXT("FAnalyticsProviderTreasureData::RecordEvent called while a session is not started. Ignoring."));
              return;
         }

         if (EventName.Len() <= 0) {
              return;
         }

         int i;
         FString Action = FString(EventName);
         FString Category = FString("");
         FString Label = FString("");
         float Value = 0;

         FString outStr;
         TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&outStr);
         JsonWriter->WriteObjectStart();

         int64 now_unix = FDateTime::Now().ToUnixTimestamp();
         if (Attributes.Num() == 0) {
           JsonWriter->WriteValue(FString("player_time"), now_unix);
           JsonWriter->WriteValue(FString("action"), EventName);
           JsonWriter->WriteObjectEnd();
           JsonWriter->Close();

           TSharedRef< IHttpRequest > HttpRequest = FHttpModule::Get().CreateRequest();
           HttpRequest->SetVerb("POST");
           HttpRequest->SetHeader("Content-Type", "application/json");
           HttpRequest->SetHeader("X-TD-Write-Key", ApiKey);
           HttpRequest->SetURL(FAnalyticsTreasureData::GetAPIURL() + Database + FString("/") + Table);
           HttpRequest->SetContentAsString(outStr);

           HttpRequest->OnProcessRequestComplete().BindRaw(this, &FAnalyticsProviderTreasureData::EventRequestComplete);
           // Execute the request
           HttpRequest->ProcessRequest();


           UE_LOG(LogAnalytics, Display, TEXT("FAnalyticsProviderTreasureData::RecordEvent Post data: %s"), *outStr);

           return;
         }

         for (i = 0; i < Attributes.Num(); i++)
           {
             if (Attributes[i].AttrName.Equals("Category") && Attributes[i].AttrValue.Len() > 0)
               {
                 Category = Attributes[i].AttrValue;
               }
             else if (Attributes[i].AttrName.Equals("Label") && Attributes[i].AttrValue.Len() > 0)
               {
                 Label = Attributes[i].AttrValue;
               }
             else if (Attributes[i].AttrName.Equals("Value"))
               {
                 Value = FCString::Atof(*Attributes[i].AttrValue);
               }

             UE_LOG(LogAnalytics, Display, TEXT("Action='%s' Category='%s' Label='%s' Value='%f'"),
                    *Action, *Category, *Label, Value);

             Category = "";
             Label = "";
             Value = 0;
           }
}

void FAnalyticsProviderTreasureData::RecordItemPurchase(const FString& ItemId, const FString& Currency, int PerItemCost, int ItemQuantity)
{
}

void FAnalyticsProviderTreasureData::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
}

void FAnalyticsProviderTreasureData::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount)
{
}

void FAnalyticsProviderTreasureData::SetAge(int InAge)
{
	Age = InAge;
}

void FAnalyticsProviderTreasureData::SetLocation(const FString& InLocation)
{
	Location = InLocation;
}

void FAnalyticsProviderTreasureData::SetGender(const FString& InGender)
{
	Gender = InGender;
}

void FAnalyticsProviderTreasureData::SetBuildInfo(const FString& InBuildInfo)
{
	BuildInfo = InBuildInfo;
}

void FAnalyticsProviderTreasureData::RecordError(const FString& Error, const TArray<FAnalyticsEventAttribute>& Attributes)
{
}

void FAnalyticsProviderTreasureData::RecordProgress(const FString& ProgressType, const FString& ProgressName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
}

void FAnalyticsProviderTreasureData::RecordItemPurchase(const FString& ItemId, int ItemQuantity, const TArray<FAnalyticsEventAttribute>& Attributes)
{
}


void FAnalyticsProviderTreasureData::RecordCurrencyPurchase(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& Attributes)
{
}


void FAnalyticsProviderTreasureData::RecordCurrencyGiven(const FString& GameCurrencyType, int GameCurrencyAmount, const TArray<FAnalyticsEventAttribute>& Attributes)
{
}

void FAnalyticsProviderTreasureData::EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid())
	{
          UE_LOG(LogAnalytics, Display, TEXT("[TD] Calc response for [%s]. Code: %d. Payload: %s"), *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
	}
	else
        {
          UE_LOG(LogAnalytics, Display, TEXT("[TD] Calc response for [%s]. No response"), *HttpRequest->GetURL());
	}
}