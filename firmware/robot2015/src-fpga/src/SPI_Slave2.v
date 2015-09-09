`ifndef _SPI_SLAVE2_
`define _SPI_SLAVE2_

module SPI_Slave2(clk, SCK, MOSI, MISO, SSEL, DONE, BYTE_TO_SEND, BYTE_RECEIVED);
input clk;
input SCK, SSEL, MOSI;
input [7:0] BYTE_TO_SEND;
output MISO;
output DONE;
output [7:0] BYTE_RECEIVED;

// sync SCK to the FPGA clock using a 3-bits shift register
reg [2:0] SCKr;  always @(posedge clk) SCKr <= {SCKr[1:0], SCK};
wire SCK_risingedge = (SCKr[2:1]==2'b01),  // now we can detect SCK rising edges
     SCK_fallingedge = (SCKr[2:1]==2'b10);  // and falling edges

// same thing for SSEL
reg [2:0] SSELr;  always @(posedge clk) SSELr <= {SSELr[1:0], SSEL};
wire SSEL_active = ~SSELr[1],
     SSEL_startmessage = (SSELr[2:1]==2'b10),  // message starts at falling edge
     SSEL_endmessage = (SSELr[2:1]==2'b01);  // message stops at rising edge

// and for MOSI
reg [1:0] MOSIr;  always @(posedge clk) MOSIr <= {MOSIr[0], MOSI};
wire MOSI_data = MOSIr[1];

// we handle SPI in 8-bits format, so we need a 3 bits counter to count the bits as they come in
reg [2:0] bitcnt;
// reg byte_received;  // high when a byte has been received
reg [7:0] byte_data_received,
          byte_rec_;
reg done_;

always @(posedge clk)
begin
  if (~SSEL_active)
    bitcnt <= 3'b000;

  else if(SCK_risingedge)
  begin
    bitcnt <= bitcnt + 1;

    // implement a shift-left register (since we receive the data MSB first)
    byte_data_received <= {byte_data_received[6:0], MOSI_data};
  end
end

always @(negedge clk) done_ <= SSEL_active && SCK_risingedge && (bitcnt==3'b111);

always @(negedge clk) byte_rec_ <= done_ ? byte_data_received : byte_rec_;

reg [7:0] byte_data_sent;

always @(posedge clk)
if(SSEL_active)
begin
  if(SSEL_startmessage)
     byte_data_sent <= BYTE_TO_SEND;  // first byte sent in a message is the message count
   else
  if(SCK_fallingedge)
  begin
    if(bitcnt==3'b111)
      byte_data_sent <= BYTE_TO_SEND;  // after that, we send 0s
    else
      byte_data_sent <= {byte_data_sent[6:0], 1'b0};
  end
end

assign MISO = byte_data_sent[7];  // send MSB first
// we assume that there is only one slave on the SPI bus
// so we don't bother with a tri-state buffer for MISO
// otherwise we would need to tri-state MISO when SSEL is inactive

assign DONE = done_;
assign BYTE_RECEIVED = byte_rec_;

endmodule

`endif
