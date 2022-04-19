#include "plugin.h"
#include "CCarCtrl.h"
#include "CWorld.h"
#if defined(GTASA)
#include "CCollision.h"
#endif
#include "CModelInfo.h"
#include "CSprite.h"
#include "CFont.h"
#include "CCamera.h"
#include "../injector/assembly.hpp"
#include "CTimer.h"
#include "CGeneral.h"
#include "IniReader/IniReader.h"

using namespace plugin;
using namespace std;
using namespace injector;

class ExtendedData {
public:
	int framesDelayCountSearchBump;
	float freeLineTimer;
	struct
	{
		unsigned char bPhysics : 1;
		unsigned char bRunGroundStuckFix : 1;
		unsigned char bFoundBump : 1;
	} flags;

	ExtendedData(CVehicle *vehicle)
	{
		freeLineTimer = 0.0f;
		framesDelayCountSearchBump = 0;
		flags.bPhysics = false;
		flags.bRunGroundStuckFix = true;
		flags.bFoundBump = false;
	}
};
VehicleExtendedData<ExtendedData> vehExData;

////////////////////////
bool cfgTestMode = true;
bool cfgOnlyInNormal = false;
bool cfgFixGroundStuck = true;
bool cfgUseBikeLogicOnCars = false;
bool cfgUseBikeLogicOnBicycles = true;
bool cfgBicyclesDontStopForRed = false;
bool cfgNoBicycleHorns = true;
bool cfgCheckObjects = false;
bool cfgSlowVehiclesSlowLane = true;
bool cfgAvoidVehiclesChangingLane = true;
float cfgTurningSpeedDecrease = 15.0f;
float cfgCruiseSpeed = 15.0f;
float cfgCruiseMinSpeed = 3.0f;
float cfgCruiseMaxSpeed = 30.0f;
float cfgFrontMultDist = 50.0f;
float cfgSidesSpeedOffsetDiv = 70.0f;
float cfgCheckGroundHeight = 0.5f;
float cfgFinalGroundHeight = 0.5f;
float cfgHeightDiffLimit = 3.0f;
float cfgAcceleration = 0.005f;
float cfgDesacceleration = 0.01f;
float cfgObstacleSpeedDecrease = 20.0f;
float cfgBicycleSpeedMult = 0.5;
float cfgBicycleCruiseMaxSpeed = 10.0f;
int cfgHornsThreshold = 100;
int cfgBumpModelStart = 0;
int cfgBumpModelEnd = 0;
int cfgSearchBumpFramesDelay = 30;
float cfgBumpSearchRadius = 6.0f;
float cfgBumpSearchDistMult = 0.3f;
float cfgBumpSpeedDiv = 1.5f;
////////////////////////
const float DEBUG_FONTSIZE = 2.5f;
const float DEBUG_LINE_HEIGHT = 0.25f;
const float DEBUG_3DTEXT_DIST = 25.0f;
////////////////////////

class VehStoredDebugVerts
{
public:
	RwIm3DVertex vertCenter[2];

	VehStoredDebugVerts()
	{
		memset(vertCenter, 0, sizeof(vertCenter));
	}
};
list<VehStoredDebugVerts*> debugVertsList;

void GetEntityDimensions(CEntity *entity, CVector *outCornerA, CVector *outCornerB)
{
#if defined(GTASA)
	CColModel *colModel = entity->GetColModel();
#else
	CColModel *colModel = CModelInfo::ms_modelInfoPtrs[entity->m_nModelIndex]->GetColModel();
#endif
	outCornerA->x = colModel->m_boundBox.m_vecMin.x;
	outCornerA->y = colModel->m_boundBox.m_vecMin.y;
	outCornerA->z = colModel->m_boundBox.m_vecMin.z;
	outCornerB->x = colModel->m_boundBox.m_vecMax.x;
	outCornerB->y = colModel->m_boundBox.m_vecMax.y;
	outCornerB->z = colModel->m_boundBox.m_vecMax.z;

	// fix
	//if (outCornerA->x > 0.0) outCornerA->x *= -1.0f; outCornerB->x *= -1.0f;
}

CVector GetWorldCoordWithOffset(CEntity *entity, CVector *offset)
{
#if defined(GTASA)
	return (Multiply3x3(*entity->m_matrix, *offset) + entity->m_matrix->pos);
#else
	return (Multiply3x3(entity->m_placement, *offset) + entity->m_placement.pos);
#endif
}

void DrawDebugStringOffset(CVehicle *vehicle, float zOffset, string text, float fontSizeMult)
{
	CVector modelMin;
	CVector modelMax;
	GetEntityDimensions(vehicle, &modelMin, &modelMax);

	CVector fontPosOffset = { 0.0, 0.0, modelMax.z + zOffset + 0.5f };
	CVector fontPos3D = GetWorldCoordWithOffset(vehicle, &fontPosOffset);
	RwV3d fontPos2D; float w, h;
#if defined(GTASA)
	if (CSprite::CalcScreenCoors({ fontPos3D.x, fontPos3D.y, fontPos3D.z}, &fontPos2D, &w, &h, true, true))
#else
	if (CSprite::CalcScreenCoors({ fontPos3D.x, fontPos3D.y, fontPos3D.z }, &fontPos2D, &w, &h, true))
#endif
	{
		float fontSizeX = w / 75.0f;
		float fontSizeY = h / 75.0f;
		if (fontSizeX > 0.2)
		{
		#if defined(GTASA)
			CFont::SetBackground(false, false);
			CFont::SetJustify(false);
			CFont::SetProportional(false);
		#else
			CFont::SetJustifyOff();
			CFont::SetBackgroundOff();
			CFont::SetPropOff();
		#endif
			CFont::SetFontStyle(2);
			CRGBA color = { 255,255,255,255 };
			CFont::SetColor(color);
			CFont::SetScale(fontSizeX, fontSizeY);
		#if defined(GTASA)
			if (fontSizeX > 0.4) CFont::SetEdge(1);
		#endif
			CFont::PrintString(fontPos2D.x, fontPos2D.y, &text[0]);
		}
	}
}

float NormalizeCruiseSpeed(float newSpeed)
{
	if (newSpeed < cfgCruiseMinSpeed) newSpeed = cfgCruiseMinSpeed;
	else if (newSpeed > cfgCruiseMaxSpeed) newSpeed = cfgCruiseMaxSpeed;
	return newSpeed;
}

#if defined(GTAVC)
int GetRandomNumberInRange(int a, int b)
{
	return a + (rand() * 0.000030517578 * (b - a));
}

void __fastcall CustomPlayHornIfNecessary(CAutomobile *_this)
{
	if (GetRandomNumberInRange(0, cfgHornsThreshold) == 0)
	{
		_this->PlayHornIfNecessary();
	}
}
#endif

#if defined(GTASA)
void __fastcall CustomPlayHornIfNecessary(CAutomobile *_this)
{
	if (CGeneral::GetRandomNumberInRange(0, cfgHornsThreshold) == 0)
	{
		if (_this->m_autoPilot.m_nCarCtrlFlags & 3)
		{
			if (cfgNoBicycleHorns && _this->m_nVehicleSubClass == VEHICLE_BMX) return;
			if (!_this->HasCarStoppedBecauseOfLight()) _this->PlayCarHorn();
		}
	}
}

struct MoreCustomPlayHornIfNecessary
{
	void operator()(reg_pack& regs)
	{
		if (CGeneral::GetRandomNumberInRange(0, cfgHornsThreshold) == 0)
		{
			CAutomobile *bike = (CAutomobile*)regs.esi;
			if (cfgNoBicycleHorns && bike->m_nVehicleSubClass == VEHICLE_BMX) return;
			bike->PlayCarHorn();
		}
	}
};
#endif

//const unsigned int FIFTEEN_FRAME_MS = (1000 / 30) / 2; // (1 sec / 30) / 2
//unsigned int lastGenerateRandomCarTime;

fstream lg;


///////////////////////////////////////////////////////////////////////////////////////////////////

bool ReadIniFloat(CIniReader ini, fstream *lg, string section, string key, float* f) {
	*f = ini.ReadFloat(section, key, -1);
	if (*f != -1) {
		*lg << /*section << ": " <<*/ key << " = " << fixed << *f << "\n";
		return true;
	}
	else return false;
}

bool ReadIniInt(CIniReader ini, fstream *lg, string section, string key, int* i) {
	*i = ini.ReadInteger(section, key, -1);
	if (*i != -1) {
		*lg << /*section << ": " <<*/ key << " = " << *i << "\n";
		return true;
	}
	else return false;
}

bool ReadIniBool(CIniReader ini, fstream *lg, string section, string key) {
	//bool b = ini.ReadBoolean(section, key, 0);
	bool b = ini.ReadInteger(section, key, 0) == 1;
	if (b == true) {
		*lg << /*section << ": " <<*/ key << " = true \n";
		return true;
	}
	else return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////


class RealTrafficFix
{
public:
    RealTrafficFix()
	{
		static constexpr float magic = 50.0f / 30.0f;

		static CPed *playerPed = nullptr;
		static int checkBumps = -1;
		static float f = 0.0f;
		static int i = 0;

		lg.open("RealTrafficFix.log", fstream::out | fstream::trunc);

		CIniReader ini("RealTrafficFix.ini");
		lg << "v2.2 beta" << endl;

		cfgTestMode					 = ReadIniBool(ini, &lg, "Settings", "TestMode");
		cfgOnlyInNormal				 = ReadIniBool(ini, &lg, "Settings", "OnlyInNormal");
		cfgFixGroundStuck			 = ReadIniBool(ini, &lg, "Settings", "FixGroundStuck");
		cfgUseBikeLogicOnCars		 = ReadIniBool(ini, &lg, "Settings", "UseBikeLogicOnCars");
	#if defined(GTASA)
		cfgUseBikeLogicOnBicycles	 = ReadIniBool(ini, &lg, "Settings", "UseBikeLogicOnBicycles");
		cfgBicyclesDontStopForRed    = ReadIniBool(ini, &lg, "Settings", "BicyclesDontStopForRed");
		cfgNoBicycleHorns			 = ReadIniBool(ini, &lg, "Settings", "NoBicycleHorns");
	#endif
		cfgCheckObjects				 = ReadIniBool(ini, &lg, "Settings", "CheckObjects");
		cfgSlowVehiclesSlowLane      = ReadIniBool(ini, &lg, "Settings", "SlowVehiclesSlowLane");
		cfgAvoidVehiclesChangingLane = ReadIniBool(ini, &lg, "Settings", "AvoidVehiclesChangingLane");
		 
		if (ReadIniFloat(ini, &lg, "Settings", "TurningSpeedDecrease", &f))			cfgTurningSpeedDecrease = f;
		if (ReadIniFloat(ini, &lg, "Settings", "CruiseSpeed", &f))					cfgCruiseSpeed = f;
		if (ReadIniFloat(ini, &lg, "Settings", "CruiseMinSpeed", &f))				cfgCruiseMinSpeed = f;
		if (ReadIniFloat(ini, &lg, "Settings", "CruiseMaxSpeed", &f))				cfgCruiseMaxSpeed = f;
		if (ReadIniFloat(ini, &lg, "Settings", "FrontMultDist", &f))				cfgFrontMultDist = f;
		if (ReadIniFloat(ini, &lg, "Settings", "SidesSpeedOffsetDiv", &f))			cfgSidesSpeedOffsetDiv = f;
		if (ReadIniFloat(ini, &lg, "Settings", "CheckGroundHeight", &f))			cfgCheckGroundHeight = f;
		if (ReadIniFloat(ini, &lg, "Settings", "FinalGroundHeight", &f))			cfgFinalGroundHeight = f;
		if (ReadIniFloat(ini, &lg, "Settings", "HeightDiffLimit", &f))				cfgHeightDiffLimit = f;
		if (ReadIniFloat(ini, &lg, "Settings", "Acceleration", &f))					cfgAcceleration = f;
		if (ReadIniFloat(ini, &lg, "Settings", "Desacceleration", &f))				cfgDesacceleration = f;
		if (ReadIniFloat(ini, &lg, "Settings", "ObstacleSpeedDecrease", &f))		cfgObstacleSpeedDecrease = f;
		if (ReadIniInt  (ini, &lg, "Settings", "HornsThreshold", &i))				cfgHornsThreshold = i;
	#if defined(GTASA)
		if (ReadIniFloat(ini, &lg, "Settings", "BicycleSpeedMult", &f))				cfgBicycleSpeedMult = f;
		if (ReadIniFloat(ini, &lg, "Settings", "BicycleCruiseMaxSpeed", &f))		cfgBicycleCruiseMaxSpeed = f;
		if (ReadIniFloat(ini, &lg, "Settings", "BumpSearchRadius", &f))				cfgBumpSearchRadius = f;
		if (ReadIniFloat(ini, &lg, "Settings", "BumpSearchDistMult", &f))			cfgBumpSearchDistMult = f;
		if (ReadIniFloat(ini, &lg, "Settings", "BumpSpeedDiv", &f))					cfgBumpSpeedDiv = f;
		if (ReadIniInt  (ini, &lg, "Settings", "BumpModelStart", &i))				cfgBumpModelStart = i;
		if (ReadIniInt  (ini, &lg, "Settings", "BumpModelEnd", &i))					cfgBumpModelEnd = i;
		if (ReadIniInt	(ini, &lg, "Settings", "SearchBumpFramesDelay", &i))		cfgSearchBumpFramesDelay = i;
	#endif

		lg.flush();

	#if defined(GTASA)
		patch::RedirectCall(0x6B5275, CustomPlayHornIfNecessary, true);
		patch::RedirectCall(0x6B52B8, CustomPlayHornIfNecessary, true);
		patch::RedirectCall(0x6C1C22, CustomPlayHornIfNecessary, true);
		patch::RedirectCall(0x6C1C6B, CustomPlayHornIfNecessary, true);
		MakeInline<MoreCustomPlayHornIfNecessary>(0x6BCD3A, 0x6BCD3A + 6);
		MakeInline<MoreCustomPlayHornIfNecessary>(0x6BCD97, 0x6BCD97 + 6);
		MakeInline<MoreCustomPlayHornIfNecessary>(0x41FD30, 0x41FD30 + 6);
	#endif
	#if defined(GTAVC)
		patch::RedirectCall(0x593D2C, CustomPlayHornIfNecessary, true);
		patch::RedirectCall(0x593D71, CustomPlayHornIfNecessary, true);
	#endif

	#if defined(GTASA)
		if (cfgAvoidVehiclesChangingLane) {
			MakeInline<0x0042E98B, 0x0042E98B + 6>([](reg_pack& regs)
			{
				CVehicle *veh = (CVehicle *)regs.esi;
				if (playerPed->m_pVehicle)
				{
					float playerCarSpeed = playerPed->m_pVehicle->m_vecMoveSpeed.Magnitude() * 50.0f * 3.6f;
					if (playerCarSpeed > 80.0f || (veh->GetPosition() - playerPed->m_pVehicle->GetPosition()).Magnitude() <= 20.0f)
					{
						return;
					}
				}
				veh->m_autoPilot.m_nNextLane = (uint8_t)regs.eax; //mov [esi+3B8h], al
			});
		}

		if (cfgSlowVehiclesSlowLane) {
			MakeInline<0x004308C9, 0x004308C9 + 12>([](reg_pack& regs)
			{
				CVehicle *veh = (CVehicle *)regs.esi;
				if ((veh->m_nVehicleFlags.bIsBig || veh->m_nVehicleFlags.bIsBus || veh->m_pHandlingData->m_transmissionData.m_fMaxGearVelocity < 0.62) &&
					(!veh->m_nVehicleFlags.bIsLawEnforcer && !veh->m_nVehicleFlags.bIsAmbulanceOnDuty && !veh->m_nVehicleFlags.bIsFireTruckOnDuty))
				{
					if (regs.ecx > 1)
					{
						veh->m_autoPilot.m_nCurrentLane = 1;
						veh->m_autoPilot.m_nNextLane = 1;
					}
					veh->m_autoPilot.m_nCarCtrlFlags |= 0x10;
				}
				else {
					veh->m_autoPilot.m_nCurrentLane = (uint8_t)regs.edx;
					veh->m_autoPilot.m_nNextLane = (uint8_t)regs.edx;
				}
			});
		}

		//injector::MakeNOP(0x434268, 5, true);
		/*
		MakeInline<0x00434263, 0x00434263 + 10>([](reg_pack& regs)
		{
			int diffFromLastTime = (CTimer::m_snTimeInMilliseconds - lastGenerateRandomCarTime);
			if (diffFromLastTime > FIFTEEN_FRAME_MS) 
			{
				do
				{
					CCarCtrl::GenerateOneRandomCar();

					diffFromLastTime -= FIFTEEN_FRAME_MS;
				} while (diffFromLastTime >= 0);

				lastGenerateRandomCarTime = CTimer::m_snTimeInMilliseconds;
			}
		});
		*/
	#endif
        
		Events::vehicleRenderEvent.before += [](CVehicle *vehicle)
		{
			if (vehicle->m_pDriver && vehicle->m_nCreatedBy != eVehicleCreatedBy::MISSION_VEHICLE)
			{
				if ((!vehicle->m_pDriver->IsPlayer() && !cfgTestMode) || (cfgTestMode))
				{
				#if defined(GTASA)
					int subClass = vehicle->m_nVehicleSubClass;
					if (subClass == VEHICLE_AUTOMOBILE ||
						subClass == VEHICLE_BIKE ||
						subClass == VEHICLE_BMX ||
						subClass == VEHICLE_MTRUCK ||
						subClass == VEHICLE_QUAD)
					{
				#else
					int subClass = vehicle->m_nVehicleClass;
					if (subClass == VEHICLE_AUTOMOBILE ||
						subClass == VEHICLE_BIKE)
					{
				#endif
						ExtendedData &xdata = vehExData.Get(vehicle);
						if (&xdata == nullptr) return;

						float curZOffset = 0.0f;
						float fNewCruiseSpeed = cfgCruiseSpeed;
						string tempText;

						// Cruise default speed
						vehicle->m_autoPilot.m_nCruiseSpeed = fNewCruiseSpeed;

					#if defined(GTASA)
						float distanceToCam = DistanceBetweenPoints(TheCamera.GetPosition(), vehicle->GetPosition());
					#else
						float distanceToCam = DistanceBetweenPoints(TheCamera.pos, vehicle->GetPosition());
					#endif

						if (cfgTestMode)
						{
							tempText = Format("dist %i", (int)distanceToCam);
							DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
							curZOffset += DEBUG_LINE_HEIGHT;
						}

						// Use real physics
						if (xdata.flags.bPhysics == false && vehicle->m_pDriver && !vehicle->m_pDriver->IsPlayer())
						{
						#if defined(GTASA)
							if (subClass == VEHICLE_AUTOMOBILE ||
								subClass == VEHICLE_BMX ||
								subClass == VEHICLE_MTRUCK)
							{
								CCarCtrl::SwitchVehicleToRealPhysics(vehicle);
								xdata.flags.bPhysics = true;
							}
						#else
							if (subClass == VEHICLE_AUTOMOBILE || subClass == VEHICLE_BIKE)
							{
								if (!(vehicle->m_nState & 1)) {
									vehicle->m_nState |= 1;
									CCarCtrl::SwitchVehicleToRealPhysics(vehicle);
								}
								xdata.flags.bPhysics = true;
							}
						#endif
						}

						// Get is bike
						bool isBike = false;
					#if defined(GTASA)
						if (subClass == VEHICLE_BIKE || subClass == VEHICLE_BMX) { isBike = true; }
					#else
						if (subClass == VEHICLE_BIKE) { isBike = true; }
					#endif

						CVector modelMin;
						CVector modelMax;
						GetEntityDimensions(vehicle, &modelMin, &modelMax);
						
						if (xdata.flags.bRunGroundStuckFix == true && cfgFixGroundStuck)
						{
						#if defined(GTASA)
							if (((vehicle->m_fMovingSpeed * (CTimer::ms_fTimeStep * magic)) < 0.5f))
						#else
							if (((vehicle->m_fTotSpeed * (CTimer::ms_fTimeStep * magic)) < 0.005f))
						#endif
							{
								if (FixGroundStuck(vehicle, &modelMin, &modelMax))
								{
									//xdata.flags.bRunGroundStuckFix = false;
								}
							}
						}

						/*if (subClass == VEHICLE_AUTOMOBILE ||
							subClass == VEHICLE_BMX ||
							subClass == VEHICLE_MTRUCK ||
							subClass == VEHICLE_QUAD)
						{*/
						//if ((vehicle->m_nStatus >> eEntityStatus::STATUS_PHYSICS) % 2 != 0) {
							// bit is set
						/*if (distanceToCam > 10.0f)
						{
							vehicle->m_nStatus = vehicle->m_nStatus & ~(1 << eEntityStatus::STATUS_PHYSICS);
							vehicle->m_autoPilot.m_nTempAction = 0;
							vehicle->m_autoPilot.m_nTimeToStartMission = CTimer::m_snTimeInMilliseconds + 2000;
							vehicle->m_autoPilot.m_nTimeSwitchedToRealPhysics = 0;
							//reinterpret_cast<CAutomobile*>(vehicle)->PlaceOnRoadProperly();

							if (cfgTestMode)
							{
								tempText = Format("NO PHYSICS");
								DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
								curZOffset += DEBUG_LINE_HEIGHT;
							}
						}*/
						//}
						//else {
							// bit is not set
						/*if (vehicle->GetIsBoundingBoxOnScreen() && distanceToCam <= 10.0f)
						{
							CCarCtrl::SwitchVehicleToRealPhysics(vehicle);
							if (cfgTestMode)
							{
								tempText = Format("USING PHYSICS");
								DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
								curZOffset += DEBUG_LINE_HEIGHT;
							}
						}*/
						//}
					//}

					/*
					if (xdata.flags.bPhysics == false)
					{
						if (subClass == VEHICLE_AUTOMOBILE ||
							subClass == VEHICLE_BMX ||
							subClass == VEHICLE_MTRUCK)
						{
							CCarCtrl::SwitchVehicleToRealPhysics(vehicle);
							xdata.flags.bPhysics = true;
						}
					}
					else {
						if (subClass == VEHICLE_AUTOMOBILE ||
							subClass == VEHICLE_BMX ||
							subClass == VEHICLE_MTRUCK)
						{

							//vehicle->m_nStatus &= ~eEntityStatus::STATUS_PHYSICS;
							//vehicle->m_autoPilot.m_nTempAction = 0;
							//vehicle->m_autoPilot.m_nTimeToStartMission = CTimer::m_snTimeInMilliseconds + 2000;
							//vehicle->m_autoPilot.m_nTimeSwitchedToRealPhysics = 0;
							xdata.flags.bPhysics = false;
						}
					}
					*/

					// Is valid driving
					#if defined(GTASA)
						if (((vehicle->m_fMovingSpeed * (CTimer::ms_fTimeStep * magic)) > 0.01f) &&
							((subClass == VEHICLE_BIKE || subClass == VEHICLE_BMX) ||
							vehicle->GetNumContactWheels() > 2))
					#else
						if (((vehicle->m_fTotSpeed * (CTimer::ms_fTimeStep * magic)) > 0.0005f))
					#endif
						{
						}
						else
						{
							xdata.freeLineTimer = 0.0f;
							return;
						}

						// I can wait
					#if defined(GTASA)
						if (vehicle->m_autoPilot.m_pTargetCar == nullptr &&
							(!vehicle->m_nVehicleFlags.bSirenOrAlarm || vehicle->m_nModelIndex == MODEL_MRWHOOP))
					#else
						if (vehicle->m_pVehicleToRam == nullptr &&
							(!vehicle->m_nSirenOrAlarm || vehicle->m_nModelIndex == 153 /*MRWHOOP*/))
					#endif
						{
						}
						else return;

						// DEBUG speed
						float speed = vehicle->m_vecMoveSpeed.Magnitude() * 50.0f * 3.6f;
						if (cfgTestMode)
						{
							tempText = Format("s %i", (int)speed);
							DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
							curZOffset += DEBUG_LINE_HEIGHT;
						}

						// Looks like it isn't stucked on the ground
						if (speed > 10.0f && distanceToCam < 30.0f) xdata.flags.bRunGroundStuckFix = false;

						// Disable exagerated driving style
					#if defined(GTASA)
						if (vehicle->m_autoPilot.m_nCarDrivingStyle == DRIVINGSTYLE_PLOUGH_THROUGH &&
							!vehicle->IsLawEnforcementVehicle())
						{
							vehicle->m_autoPilot.m_nCarDrivingStyle = DRIVINGSTYLE_AVOID_CARS;
						}

						// Make use of driving style "6" for cars and bicycles

						if (vehicle->m_autoPilot.m_nCarDrivingStyle <= 2 || vehicle->m_autoPilot.m_nCarDrivingStyle == 4) // ok to change
						{
							if (!isBike)
							{
								if (cfgUseBikeLogicOnCars)
								{
									if (reinterpret_cast<CAutomobile*>(vehicle)->HasCarStoppedBecauseOfLight())
									{
										vehicle->m_autoPilot.m_nCarDrivingStyle = DRIVINGSTYLE_STOP_FOR_CARS;
									}
									else
									{
										vehicle->m_autoPilot.m_nCarDrivingStyle = (eCarDrivingStyle)6;
									}
								}
							}
							else
							{
								if (vehicle->m_nVehicleSubClass == VEHICLE_BMX)
								{
									if (cfgUseBikeLogicOnBicycles)
									{
										vehicle->m_autoPilot.m_nCarDrivingStyle = (eCarDrivingStyle)6;
									}
									if (cfgBicyclesDontStopForRed)
									{
										vehicle->m_autoPilot.m_nCarDrivingStyle = eCarDrivingStyle::DRIVINGSTYLE_PLOUGH_THROUGH;
									}
								}
							}
						}

						// DEBUG drive style
						if (cfgTestMode)
						{
							switch (vehicle->m_autoPilot.m_nCarDrivingStyle)
							{
							case eCarDrivingStyle::DRIVINGSTYLE_STOP_FOR_CARS:
								tempText = "STOP_FOR_CARS";
								break;
							case eCarDrivingStyle::DRIVINGSTYLE_SLOW_DOWN_FOR_CARS:
								tempText = "SLOW_DOWN_FOR_CARS";
								break;
							case eCarDrivingStyle::DRIVINGSTYLE_AVOID_CARS:
								tempText = "AVOID_CARS";
								break;
							case eCarDrivingStyle::DRIVINGSTYLE_PLOUGH_THROUGH:
								tempText = "PLOUGH_THROUGH";
								break;
							case eCarDrivingStyle::DRIVINGSTYLE_STOP_FOR_CARS_IGNORE_LIGHTS:
								tempText = "STOP_FOR_CARS_IGNORE_LIGHTS";
								break;
							default:
								tempText = Format("%i", (int)vehicle->m_autoPilot.m_nCarDrivingStyle);
								break;
							}
							DrawDebugStringOffset(vehicle, curZOffset, tempText, 0.6f);
							curZOffset += DEBUG_LINE_HEIGHT;
						}
					#endif

						// Only in normal behaviour
					#if defined(GTASA)
						if (cfgOnlyInNormal && vehicle->m_autoPilot.m_nCarDrivingStyle != DRIVINGSTYLE_STOP_FOR_CARS)
					#else
						if (cfgOnlyInNormal && vehicle->m_autoPilot.m_nDrivingStyle != DRIVINGSTYLE_STOP_FOR_CARS)
					#endif
						{
							return;
						}

						// Check reverse gear
						if (vehicle->m_nCurrentGear <= 0)
						{
							return;
						}

						// Decrease speed turning
						float absSteerAngle = abs(vehicle->m_fSteerAngle);
						if (absSteerAngle > 0.001)
						{
							float turningDecrease = (cfgTurningSpeedDecrease * absSteerAngle);
							// DEBUG turning
							if (cfgTestMode)
							{
								tempText = Format("td %.2f", turningDecrease);
								DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
								curZOffset += DEBUG_LINE_HEIGHT;
							}
							fNewCruiseSpeed = NormalizeCruiseSpeed(fNewCruiseSpeed - turningDecrease);
						}

						float speedFactor = speed / 40.0f;
						if (speedFactor > 1.0f) speedFactor = 1.0f;

						float dist = vehicle->m_vecMoveSpeed.Magnitude() * cfgFrontMultDist;
						if (dist < 1.0f) dist = 1.0f;

						bool foundGround = false;
						bool forceObstacle = false;
						float newZ;
						float steerBackOffset;

						CVector offsetA[3];
						CVector coordA[3];
						CVector coordB[3];
						CVector offset;

						// Get front lines for collision detection
						// Bikes and bicycles uses only center

						float steerOffset = vehicle->m_fSteerAngle * (20.0f * speedFactor) * -1.0f;
						//if (steerOffset > 2.0f) steerOffset = 2.0f;
						//else if (steerOffset < -2.0) steerOffset = -2.0f;

						float frontHeight = (modelMin.z * 0.4f);
						if (frontHeight > 1.0f) frontHeight = 1.0f;
						else if (frontHeight < -1.0f) frontHeight = -1.0f;

						float frontHeightBonus = 0.15f;
						if (isBike) frontHeightBonus = 0.5f;

						float heightDiffLimit = cfgHeightDiffLimit;
						if (isBike) heightDiffLimit *= 2.0f;

						// C
						offset = { 0.0, modelMax.y, (frontHeight + frontHeightBonus) };
						coordA[0] = GetWorldCoordWithOffset(vehicle, &offset);

						offset = { steerOffset, (modelMax.y + dist), ((frontHeight / 1.5f) + (frontHeightBonus * 2.0f)) };
						coordB[0] = GetWorldCoordWithOffset(vehicle, &offset) ;
					#if defined(GTASA)
						newZ = CWorld::FindGroundZFor3DCoord(coordB[0].x, coordB[0].y, coordB[0].z + cfgCheckGroundHeight, &foundGround, nullptr);
					#else
						newZ = CWorld::FindGroundZFor3DCoord(coordB[0].x, coordB[0].y, coordB[0].z + cfgCheckGroundHeight, &foundGround);
					#endif
						if (abs((newZ - coordB[0].z)) > heightDiffLimit) forceObstacle = true; else coordB[0].z = newZ + cfgFinalGroundHeight;

						if (!isBike)
						{
							// L
							steerBackOffset = (modelMax.y * vehicle->m_fSteerAngle);
							if (steerBackOffset < 0.0f) steerBackOffset = 0.0f;
							offsetA[1] = { modelMin.x, modelMax.y - steerBackOffset, frontHeight + frontHeightBonus };
							coordA[1] = GetWorldCoordWithOffset(vehicle, &offsetA[1]);

							offset = { modelMin.x - (speed / cfgSidesSpeedOffsetDiv) + steerOffset, (modelMax.y + dist), (frontHeight / 1.5f) };
							coordB[1] = GetWorldCoordWithOffset(vehicle, &offset);
						#if defined(GTASA)
							newZ = CWorld::FindGroundZFor3DCoord(coordB[1].x, coordB[1].y, coordB[1].z + cfgCheckGroundHeight, &foundGround, nullptr);
						#else
							newZ = CWorld::FindGroundZFor3DCoord(coordB[1].x, coordB[1].y, coordB[1].z + cfgCheckGroundHeight, &foundGround);
						#endif
							if (abs((newZ - coordB[1].z)) > cfgHeightDiffLimit) forceObstacle = true; else coordB[1].z = newZ + cfgFinalGroundHeight;
							

							// R
							steerBackOffset = (modelMax.y * vehicle->m_fSteerAngle);
							if (steerBackOffset > 0.0f) steerBackOffset = 0.0f;
							offset = { modelMax.x, modelMax.y + steerBackOffset, frontHeight + frontHeightBonus };
							coordA[2] = GetWorldCoordWithOffset(vehicle, &offset);

							offset = { modelMax.x + (speed / cfgSidesSpeedOffsetDiv) + steerOffset, (modelMax.y + dist), (frontHeight / 1.5f) };
							coordB[2] = GetWorldCoordWithOffset(vehicle, &offset);
						#if defined(GTASA)
							newZ = CWorld::FindGroundZFor3DCoord(coordB[2].x, coordB[2].y, coordB[2].z + cfgCheckGroundHeight, &foundGround, nullptr);
						#else
							newZ = CWorld::FindGroundZFor3DCoord(coordB[2].x, coordB[2].y, coordB[2].z + cfgCheckGroundHeight, &foundGround);
						#endif
							if (abs((newZ - coordB[2].z)) > cfgHeightDiffLimit) forceObstacle = true; else coordB[2].z = newZ + cfgFinalGroundHeight;
						}

						// DEBUG model size
						/*if (cfgTestMode)
						{
							tempText = Format("mnz %.2f", modelMin.z);
							DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
							curZOffset += DEBUG_LINE_HEIGHT;
						}*/

						VehStoredDebugVerts *verts[3];
						RwRGBA c[3];
						if (cfgTestMode)
						{
							verts[0] = new VehStoredDebugVerts();
							c[0] = { 0,255,0 };
							if (!isBike)
							{
								verts[1] = new VehStoredDebugVerts();
								c[1] = { 0,255,0 };
								verts[2] = new VehStoredDebugVerts();
								c[2] = { 0,255,0 };
							}
						}

						float obstacleDistFactor;
						bool bObstacle = false;
						CWorld::pIgnoreEntity = vehicle;
						//if (!CWorld::GetIsLineOfSightClear(coordA, coordB, true, true, true, true, false, false, false)) { c = { 255,0,0 }; }
						CColPoint outColPoint;
						CEntity *outEntityC = nullptr;
						CEntity *outEntityL = nullptr;
						CEntity *outEntityR = nullptr;

						if (forceObstacle) {
							obstacleDistFactor = 1.0f;
							goto label_force_obstacle;
						}

						if (isBike)
						{
							if (CWorld::ProcessLineOfSight(coordA[0], coordB[0], outColPoint, outEntityC, 1, 1, 1, cfgCheckObjects, 0, 0, 0, 0))
							{
								goto label_obstacle;
							}
							goto label_force_no_obstacle;
						}

						if (CWorld::ProcessLineOfSight(coordA[0], coordB[0], outColPoint, outEntityC, 1, 1, 1, cfgCheckObjects, 0, 0, 0, 0) ||
							CWorld::ProcessLineOfSight(coordA[1], coordB[1], outColPoint, outEntityL, 1, 1, 1, cfgCheckObjects, 0, 0, 0, 0) ||
							CWorld::ProcessLineOfSight(coordA[2], coordB[2], outColPoint, outEntityR, 1, 1, 1, cfgCheckObjects, 0, 0, 0, 0))
						{
						label_obstacle:

						#if defined(GTASA)
							obstacleDistFactor = DistanceBetweenPoints(vehicle->GetPosition(), outColPoint.m_vecPoint) / 20.0f;
						#else
							obstacleDistFactor = DistanceBetweenPoints(vehicle->GetPosition(), outColPoint.m_vPoint) / 20.0f;
						#endif
							if (obstacleDistFactor > 1.0f) obstacleDistFactor = 1.0f;
							obstacleDistFactor = 1.0f - obstacleDistFactor;

						label_force_obstacle:

							xdata.freeLineTimer -= ((obstacleDistFactor * speedFactor) * (CTimer::ms_fTimeStep / magic));
							if (xdata.freeLineTimer < 0.0f) xdata.freeLineTimer = 0.0f;

							//float obstacleSpeedDecrease = ((obstacleDistFactor * speedFactor * 4.0f) * (CTimer::ms_fTimeStep / magic));
							//fNewCruiseSpeed = NormalizeCruiseSpeed(fNewCruiseSpeed - obstacleSpeedDecrease);
							fNewCruiseSpeed = NormalizeCruiseSpeed(vehicle->m_autoPilot.m_nCruiseSpeed - ((cfgObstacleSpeedDecrease * obstacleDistFactor) * (CTimer::ms_fTimeStep / magic)));

							// DEBUG free line timer
							if (cfgTestMode)
							{
								if (forceObstacle) tempText = Format("od -%.2f (FORCED)", xdata.freeLineTimer);
								else tempText = Format("od -%.2f", xdata.freeLineTimer);
								DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
								curZOffset += DEBUG_LINE_HEIGHT;

								// Set lines to red
								if (outEntityC || forceObstacle) c[0] = { 255,0,0 };
								if (outEntityL || forceObstacle) c[1] = { 255,0,0 };
								if (outEntityR || forceObstacle) c[2] = { 255,0,0 };
							}
						}
						else // no obstacle
						{
						label_force_no_obstacle:

						#if defined(GTASA)
							CVector bumpSearchCoord;
							float finalBumpSearchRadius;
							if (checkBumps == true) {
								offset = { 0.0f, (modelMax.y + (dist * cfgBumpSearchDistMult)), 0.0f };
								bumpSearchCoord = GetWorldCoordWithOffset(vehicle, &offset);
								if (isBike) finalBumpSearchRadius = (cfgBumpSearchRadius * 0.8f) + abs(modelMin.y);
								else finalBumpSearchRadius = (cfgBumpSearchRadius + abs(modelMin.y));
							}

							if (checkBumps == true && ((++xdata.framesDelayCountSearchBump < cfgSearchBumpFramesDelay && xdata.flags.bFoundBump) || (xdata.framesDelayCountSearchBump >= cfgSearchBumpFramesDelay && ThereIsObjectHere(bumpSearchCoord, finalBumpSearchRadius, cfgBumpModelStart, cfgBumpModelEnd)))) {
								if (xdata.framesDelayCountSearchBump >= cfgSearchBumpFramesDelay) {
									xdata.framesDelayCountSearchBump = 0;
									xdata.flags.bFoundBump = true;
								}
								xdata.freeLineTimer = 0.0f;
								fNewCruiseSpeed /= cfgBumpSpeedDiv;

								// DEBUG there is bump
								if (cfgTestMode)
								{
									tempText = Format("bump");
									DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
									curZOffset += DEBUG_LINE_HEIGHT;
								}
							}
							else
							{
								if (checkBumps) xdata.flags.bFoundBump = false;
								if (vehicle->m_fGasPedal >= 0.05f && !vehicle->m_nVehicleFlags.bParking)
							#else
								if (vehicle->m_fGasPedal >= 0.05f)
							#endif
								{ // accelerate
									xdata.freeLineTimer += (cfgAcceleration * (CTimer::ms_fTimeStep / magic));
									if (xdata.freeLineTimer > 1.0f) xdata.freeLineTimer = 1.0f;
									float freeLineSpeedIncrease = (cfgCruiseMaxSpeed - cfgCruiseSpeed) * xdata.freeLineTimer;
									fNewCruiseSpeed = NormalizeCruiseSpeed(fNewCruiseSpeed + freeLineSpeedIncrease);

									// DEBUG free line timer
									if (cfgTestMode)
									{
										tempText = Format("fi +%.2f", xdata.freeLineTimer);
										DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
										curZOffset += DEBUG_LINE_HEIGHT;
									}
								}
								else
								{ // I don't want to accelerate
									xdata.freeLineTimer -= (cfgDesacceleration * (CTimer::ms_fTimeStep / magic));
									if (xdata.freeLineTimer < 0.0f) xdata.freeLineTimer = 0.0f;

									// DEBUG free line timer
									if (cfgTestMode)
									{
										tempText = Format("fd -%.2f", xdata.freeLineTimer);
										DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
										curZOffset += DEBUG_LINE_HEIGHT;
									}
								}
						#if defined(GTASA)
							}
						#endif
						}

						if (cfgTestMode)
						{
							// DEBUG final cruise speed
							if (cfgTestMode)
							{
								tempText = Format("fs %.1f", fNewCruiseSpeed);
								DrawDebugStringOffset(vehicle, curZOffset, tempText, 1.0f);
								curZOffset += DEBUG_LINE_HEIGHT;
							}

							// Store debug lines
							// C
							RwIm3DVertexSetPos(&verts[0]->vertCenter[0], coordA[0].x, coordA[0].y, coordA[0].z);
							RwIm3DVertexSetPos(&verts[0]->vertCenter[1], coordB[0].x, coordB[0].y, coordB[0].z);
							RwIm3DVertexSetRGBA(&verts[0]->vertCenter[0], c[0].red, c[0].green, c[0].blue, 255);
							RwIm3DVertexSetRGBA(&verts[0]->vertCenter[1], c[0].red, c[0].green, c[0].blue, 255);
							debugVertsList.push_back(verts[0]);

							if (!isBike)
							{
								//L
								RwIm3DVertexSetPos(&verts[1]->vertCenter[0], coordA[1].x, coordA[1].y, coordA[1].z);
								RwIm3DVertexSetPos(&verts[1]->vertCenter[1], coordB[1].x, coordB[1].y, coordB[1].z);
								RwIm3DVertexSetRGBA(&verts[1]->vertCenter[0], c[1].red, c[1].green, c[1].blue, 255);
								RwIm3DVertexSetRGBA(&verts[1]->vertCenter[1], c[1].red, c[1].green, c[1].blue, 255);
								debugVertsList.push_back(verts[1]);

								//R
								RwIm3DVertexSetPos(&verts[2]->vertCenter[0], coordA[2].x, coordA[2].y, coordA[2].z);
								RwIm3DVertexSetPos(&verts[2]->vertCenter[1], coordB[2].x, coordB[2].y, coordB[2].z);
								RwIm3DVertexSetRGBA(&verts[2]->vertCenter[0], c[2].red, c[2].green, c[2].blue, 255);
								RwIm3DVertexSetRGBA(&verts[2]->vertCenter[1], c[2].red, c[2].green, c[2].blue, 255);
								debugVertsList.push_back(verts[2]);
							}
						}
						 
						// Stress test
						/*VehStoredDebugVerts *verts;
						for (float f = -2.0; f <= 2.0; f += 0.025)
						{
							verts = new VehStoredDebugVerts();
							RwRGBA c = { 0,255,0 };

							CVector offsetA = { f, modelMax.y, 0.0 };
							CVector coordA = GetWorldCoordWithOffset(vehicle, &offsetA);
							CVector offsetB = { f, (modelMax.y + dist), 0.0 };
							CVector coordB = GetWorldCoordWithOffset(vehicle, &offsetB);

							CColPoint outColPoint;
							CEntity *outEntity;
							CWorld::pIgnoreEntity = vehicle;
							if (CWorld::ProcessLineOfSight(coordA, coordB, outColPoint, outEntity, 1, 1, 1, 1, 0, 0, 0, 0)) { c = { 255,0,0 }; }
							//if (!CWorld::GetIsLineOfSightClear(coordA, coordB, true, true, true, true, false, false, false)) { c = { 255,0,0 }; }

							RwIm3DVertexSetPos(&verts->vertCenter[0], coordA.x, coordA.y, coordA.z);
							RwIm3DVertexSetPos(&verts->vertCenter[1], coordB.x, coordB.y, coordB.z);
							RwIm3DVertexSetRGBA(&verts->vertCenter[0], c.red, c.green, c.blue, 255);
							RwIm3DVertexSetRGBA(&verts->vertCenter[1], c.red, c.green, c.blue, 255);
							debugVertsList.push_back(verts);
						}*/

						// Set final cruise speed
					#if defined(GTASA)
						if (subClass == VEHICLE_BMX)
						{
							fNewCruiseSpeed *= cfgBicycleSpeedMult;
							if (fNewCruiseSpeed > cfgBicycleCruiseMaxSpeed) fNewCruiseSpeed = cfgBicycleCruiseMaxSpeed;
						}
					#endif
						vehicle->m_autoPilot.m_nCruiseSpeed = (char)fNewCruiseSpeed;
					}
				}
			}
		};

		Events::processScriptsEvent.after += []
		{
			playerPed = FindPlayerPed();
		#if defined(GTASA)
			if (checkBumps == -1)
			{
				checkBumps = false;
				if (cfgBumpModelStart > 0 && (cfgBumpModelEnd > cfgBumpModelStart) && cfgBumpSearchRadius > 0.0f && cfgBumpSearchDistMult > 0.0f && cfgBumpSpeedDiv > 0.0f) {
					if (plugin::CallAndReturn<bool, 0x407800, int>(cfgBumpModelStart) && plugin::CallAndReturn<bool, 0x407800, int>(cfgBumpModelEnd)) {
						checkBumps = true;
						lg << "Bumps is installed and will be checked\n";
						lg.flush();
					}
				}
			}
		#endif
		};

		Events::drawingEvent.Add([]
		{
			RwEngineInstance->dOpenDevice.fpRenderStateSet(rwRENDERSTATEZTESTENABLE, FALSE);

			for (list<VehStoredDebugVerts*>::iterator it = debugVertsList.begin(); it != debugVertsList.end(); ++it)
			{
				VehStoredDebugVerts *verts = *it;
				if (RwIm3DTransform(verts->vertCenter, 2, 0, 0)) {
					RwIm3DRenderLine(0, 1);
					RwIm3DEnd();
				}
			}

			RwEngineInstance->dOpenDevice.fpRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);

			debugVertsList.clear();
		});

	#if defined(GTASA)
		Events::onPauseAllSounds +=[] {
			lg.flush();
		};
	#endif
    }

	static bool ThereIsObjectHere(CVector coord, float radius, int modelStart, int modelEnd)
	{
		for (int index = 0; index < CPools::ms_pObjectPool->m_nSize; ++index)
		{
			if (auto obj = CPools::ms_pObjectPool->GetAt(index))
			{
				//lg << obj->m_nModelIndex << " dist " << (obj->GetPosition() - coord).Magnitude() << "\n";
				if (obj->m_nModelIndex >= modelStart && obj->m_nModelIndex <= modelEnd)
				{
					if ((obj->GetPosition() - coord).Magnitude() <= radius)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	static bool FixGroundStuck_IsValid(CVector coord, float maxZ)
	{
		bool foundGround;
		coord.z += maxZ;
	#if defined(GTASA)
		float groundZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround, nullptr);
	#else
		float groundZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround);
	#endif
		if (foundGround) return true;
		return false;
	}

	static bool FixGroundStuck(CVehicle *vehicle, CVector *modelMin, CVector *modelMax)
	{
		CVector offset;
		CVector coord;
		float groundZ;
		bool foundGround;

		offset = { 0.0, (modelMin->y * 0.9f), 0.0f };
		coord = GetWorldCoordWithOffset(vehicle, &offset);
	#if defined(GTASA)
		groundZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround, nullptr);
	#else
		groundZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround);
	#endif
		if (!foundGround && FixGroundStuck_IsValid(coord, modelMax->z)) {
			if (ReposVehicle(vehicle)) return true; else return false;
		}

		offset = { 0.0, (modelMax->y * 0.9f), 0.0f };
		coord = GetWorldCoordWithOffset(vehicle, &offset);
	#if defined(GTASA)
		groundZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround, nullptr);
	#else
		groundZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround);
	#endif
		if (!foundGround && FixGroundStuck_IsValid(coord, modelMax->z)) {
			if (ReposVehicle(vehicle)) return true; else return false;
		}

		return false;

		/*
		0441: 5@ = car hVEH model
		07E4: get_model 5@ dimensions_cornerA_to 5@ 12@ 5@ dimensions_cornerB_to 5@ 22@ 5@
		12@ *= 0.9                                                                         
		22@ *= 0.9
		0407: store_coords_to 11@ 12@ 13@ from_car hVEH with_offset 0.0 12@ 0.0     
		0407: store_coords_to 21@ 22@ 23@ from_car hVEH with_offset 0.0 22@ 0.0

		02CE: 5@ = ground_z_at 11@ 12@ 13@ 
		02CE: 26@ = ground_z_at 21@ 22@ 23@

		if or
		5@ == 0.00000  
		26@ == 0.00000
		then
			//again, to make sure that isn't a hole in coll
			11@ += 0.01
			12@ += 0.01
			21@ += 0.01
			22@ += 0.01
			02CE: 5@ = ground_z_at 11@ 12@ 13@ 
			02CE: 26@ = ground_z_at 21@ 22@ 23@
			if or 
			5@ == 0.00000
			26@ == 0.00000
			then //ok, convinced me
				00AA: store_car hVEH position_to 11@ 12@ 13@
				13@ += 3.0
				0208: 5@ = random_float_in_ranges -0.1 0.1
				11@ += 5@ 
				12@ += 5@
				00AB: put_car hVEH at 11@ 12@ -100.0
			end
		end
		return
					*/
	}
	 
	static bool ReposVehicle(CVehicle* vehicle)
	{
		float randomDiff;
		bool foundGround;
		float newZ;
		CVector coord = vehicle->GetPosition();

		for (int i = 0; i < 2; i++)
		{
			lg << "Fixing ground stuck\n";
		#if defined(GTASA)
			randomDiff = CGeneral::GetRandomNumberInRange(-0.5f, 0.5f);
		#else
			randomDiff = 0.027f;
		#endif
			coord.x += randomDiff;
			coord.y += randomDiff;
			coord.z += 2.0f;

		#if defined(GTASA)
			newZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround, nullptr);
		#else
			newZ = CWorld::FindGroundZFor3DCoord(coord.x, coord.y, coord.z, &foundGround);
		#endif
			if (foundGround)
			{
				coord.z = newZ + 0.5f;

				lg << "FIXED\n";

			#if defined(GTASA)
				vehicle->SetPosn(coord);
			#else
				vehicle->SetPosition(coord);
			#endif
				reinterpret_cast<CAutomobile*>(vehicle)->PlaceOnRoadProperly();
				//lg.flush();
				return true;
			}
		}
		//lg.flush();
		return false;
	}
} realTrafficFix;
