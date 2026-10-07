// Add to your testbench so the RTL writes a trace in the same format as the ISS.
// Rename the signals (dut.pc, dut.instr, ...) to match YOUR core.
integer trace_f;
initial trace_f = $fopen("rtl_trace.txt", "w");

always @(posedge clk) begin
  if (!reset) begin                        // only trace after reset is released
    if (dut.reg_write && dut.rd != 5'd0)  // instruction writes a register (not x0)
      $fdisplay(trace_f, "%08x %08x x%0d=%08x", dut.pc, dut.instr, dut.rd, dut.rd_wdata);
    else
      $fdisplay(trace_f, "%08x %08x", dut.pc, dut.instr);
  end
end
// Remember to $fclose(trace_f) before $finish.
