int SEGGER_RTT_printf(unsigned BufferIndex, const char *sFormat, ...) // NOSONAR - mocking external function prototype
{
   (void)BufferIndex;
   (void)sFormat;
   return 0;
}