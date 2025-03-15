	R(  1,  APBC,        UART1, __BIT(2), 0),
	R(  2,  APBC,        UART2, __BIT(2), 0),
	R(  3,  APBC,         GPIO, __BIT(2), 0),
	R(  4,  APBC,         PWM0, __BIT(2), __BIT( 0)),
	R(  5,  APBC,         PWM1, __BIT(2), __BIT( 0)),
	R(  6,  APBC,         PWM2, __BIT(2), __BIT( 0)),
	R(  7,  APBC,         PWM3, __BIT(2), __BIT( 0)),
	R(  8,  APBC,         PWM4, __BIT(2), __BIT( 0)),
	R(  9,  APBC,         PWM5, __BIT(2), __BIT( 0)),
	R( 10,  APBC,         PWM6, __BIT(2), __BIT( 0)),
	R( 11,  APBC,         PWM7, __BIT(2), __BIT( 0)),
	R( 12,  APBC,         PWM8, __BIT(2), __BIT( 0)),
	R( 13,  APBC,         PWM9, __BIT(2), __BIT( 0)),
	R( 14,  APBC,        PWM10, __BIT(2), __BIT( 0)),
	R( 15,  APBC,        PWM11, __BIT(2), __BIT( 0)),
	R( 16,  APBC,        PWM12, __BIT(2), __BIT( 0)),
	R( 17,  APBC,        PWM13, __BIT(2), __BIT( 0)),
	R( 18,  APBC,        PWM14, __BIT(2), __BIT( 0)),
	R( 19,  APBC,        PWM15, __BIT(2), __BIT( 0)),
	R( 20,  APBC,        PWM16, __BIT(2), __BIT( 0)),
	R( 21,  APBC,        PWM17, __BIT(2), __BIT( 0)),
	R( 22,  APBC,        PWM18, __BIT(2), __BIT( 0)),
	R( 23,  APBC,        PWM19, __BIT(2), __BIT( 0)),
	R( 24,  APBC,         SSP3, __BIT(2), 0),
	R( 25,  APBC,        UART3, __BIT(2), 0),
	R( 26,  APBC,          RTC, __BIT(2), 0),
	R( 27,  APBC,        TWSI0, __BIT(2), 0),
	R( 28,  APBC,      TIMERS1, __BIT(2), 0),
	R( 29,  APBC,          AIB, __BIT(2), 0),
	R( 30,  APBC,      TIMERS2, __BIT(2), 0),
	R( 31,  APBC,      ONEWIRE, __BIT(2), 0),
	R( 32,  APBC,        SSPA0, __BIT(2), 0),
	R( 33,  APBC,        SSPA1, __BIT(2), 0),
	R( 34,  APBC,          DRO, __BIT(2), 0),
	R( 35,  APBC,           IR, __BIT(2), 0),
	R( 36,  APBC,        TWSI1, __BIT(2), 0),
	R( 37,  APBC,         TSEN, __BIT(2), 0),
	R( 38,  APBC,        TWSI2, __BIT(2), 0),
	R( 39,  APBC,        TWSI4, __BIT(2), 0),
	R( 40,  APBC,        TWSI5, __BIT(2), 0),
	R( 41,  APBC,        TWSI6, __BIT(2), 0),
	R( 42,  APBC,        TWSI7, __BIT(2), 0),
	R( 43,  APBC,        TWSI8, __BIT(2), 0),
	R( 44,  APBC,   IPC_AP2AUD, __BIT(2), 0),
	R( 45,  APBC,        UART4, __BIT(2), 0),
	R( 46,  APBC,        UART5, __BIT(2), 0),
	R( 47,  APBC,        UART6, __BIT(2), 0),
	R( 48,  APBC,        UART7, __BIT(2), 0),
	R( 49,  APBC,        UART8, __BIT(2), 0),
	R( 50,  APBC,        UART9, __BIT(2), 0),
	R( 51,  APBC,         CAN0, __BIT(2), 0),
	R( 52,  MPMU,       WDTPCR, __BIT(2), 0),
	R( 53,  APMU,          JPG, 0,        __BIT( 0)),
	R( 54,  APMU,    CSI_CCIC2, 0,        __BIT( 1)),	// CSI
	R( 55,  APMU,    CSI_CCIC2, 0,        __BIT( 2)),	// CCIC2_PHY
	R( 56,  APMU,    CSI_CCIC2, 0,        __BIT(29)),	// CCIC3_PHY
	R( 57,  APMU,          ISP, 0,        __BIT( 0)),
	R( 58,  APMU,          ISP, 0,        __BIT( 3)),	// AHB
	R( 59,  APMU,          ISP, 0,        __BIT(16)),	// CI
	R( 60,  APMU,          ISP, 0,        __BIT(27)),	// CPP
	R( 61,  APMU,         LCD1, 0,        __BIT( 4)),
	R( 62,  APMU,         LCD1, 0,        __BIT( 3)),	// DSI_ESC
	R( 63,  APMU,         LCD1, 0,        __BIT(27)),	// V2D
	R( 64,  APMU,         LCD1, 0,        __BIT(15)),	// MIPI
	R( 65,  APMU,      LCD_SPI, 0,        __BIT( 0)),
	R( 66,  APMU,      LCD_SPI, 0,        __BIT( 4)),	// BUS
	R( 67,  APMU,      LCD_SPI, 0,        __BIT( 2)),	// HBUS
	R( 68,  APMU,         LCD2, 0,        __BIT( 9)),	// LCLK
	R( 69,  APMU,         CCIC, 0,        __BIT( 1)),	// 4X
	R( 70,  APMU,         CCIC, 0,        __BIT( 2)),	// 1_PHY
	R( 71,  APMU,         SDH0, 0,        __BIT( 0)),	// AXI
	R( 72,  APMU,         SDH0, 0,        __BIT( 1)),
	R( 73,  APMU,         SDH1, 0,        __BIT( 1)),
	R( 74,  APMU,          USB, 0,        __BIT( 0)),	// AXI
	R( 75,  APMU,          USB, 0,        __BIT( 4)),	// P1_AXI
	R( 76,  APMU,          USB, 0,        __BITS(9,11)),	// 3_1
	R( 77,  APMU,         QSPI, 0,        __BIT( 1)),
	R( 78,  APMU,         QSPI, 0,        __BIT( 0)),	// BUS
	R( 79,  APMU,          DMA, 0,        __BIT( 0)),
	R( 80,  APMU,          AES, 0,        __BIT( 4)),
	R( 81,  APMU,          VPU, 0,        __BIT( 0)),
	R( 82,  APMU,          GPU, 0,        __BIT( 1)),
	R( 83,  APMU,         SDH2, 0,        __BIT( 1)),
	R( 84,  APMU, PMUA_MC_CTRL, 0,        __BIT( 0)),
	R( 85,  APMU,      PMUA_EM, 0,        __BIT( 0)),	// AXI
	R( 86,  APMU,      PMUA_EM, 0,        __BIT( 1)),
	R( 87,  APMU,        AUDIO, 0,        __BIT(0)|__BITS(2,3)),
	R( 88,  APMU,         HDMI, 0,        __BIT( 9)),
	R( 89,  APMU,        PCIE0, __BIT(8), __BITS(3,5)),
	R( 90,  APMU,        PCIE1, __BIT(8), __BITS(3,5)),
	R( 91,  APMU,        PCIE2, __BIT(8), __BITS(3,5)),
	R( 92,  APMU,        EMAC0, 0,        __BIT( 1)),
	R( 93,  APMU,        EMAC1, 0,        __BIT( 1)),
	R( 94, APBC2,        UART1, __BIT(2), 0),
	R( 95, APBC2,         SSP2, __BIT(2), 0),
	R( 96, APBC2,        TWSI3, __BIT(2), 0),
	R( 97, APBC2,          RTC, __BIT(2), 0),
	R( 98, APBC2,      TIMERS0, __BIT(2), 0),
	R( 99, APBC2,          KPC, __BIT(2), 0),
	R(100, APBC2,         GPIO, __BIT(2), 0),
	R(101,  RCPU,         HDMI, 0,        __BIT( 0)),
	R(102,  RCPU,          CAN, 0,        __BIT( 0)),
	R(103, RCPU2,          PWM, __BIT(2), __BIT( 0)),
