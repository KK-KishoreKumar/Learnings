#define TX_BUF_SIZE  1536  /* should be at least MTU + 14 + 4 */
#define TOTAL_TX_BUF_SIZE  (TX_BUF_SIZE * NUM_TX_SIZE)

/* 8139 register offsets */
#define TSD0      0x10
#define TSAD0     0x20
#define RBSTART   0x30
#define CR        0x37
#define CAPR      0x38
#define IMR       0x3c
#define ISR       0x3e
#define TCR       0x40
#define RCR       0x44
#define MPC       0x4c
#define MULINT    0x5c

/* TSD register commands */
#define TxHostOwns    0x2000
#define TxUnderrun    0x4000
#define TxStatOK      0x8000
#define TxOutOfWindow 0x20000000
#define TxAborted     0x40000000
#define TxCarrierLost 0x80000000

/* CR register commands */
#define RxBufEmpty 0x01
#define CmdTxEnb   0x04
#define CmdRxEnb   0x08
#define CmdReset   0x10

/* ISR Bits */
#define RxOK       0x01
#define RxErr      0x02
#define TxOK       0x04
#define TxErr      0x08
#define RxOverFlow 0x10
#define RxUnderrun 0x20
#define RxFIFOOver 0x40
#define CableLen   0x2000
#define TimeOut    0x4000
#define SysErr     0x8000

#define INT_MASK (RxOK | RxErr | TxOK | TxErr | \
		               RxOverFlow | RxUnderrun | RxFIFOOver | \
		               CableLen | TimeOut | SysErr)
