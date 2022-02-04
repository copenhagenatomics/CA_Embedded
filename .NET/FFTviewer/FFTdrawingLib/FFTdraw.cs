using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;

namespace FFTdrawingLib
{
#pragma warning disable CA1416 // Validate platform compatibility
    public class FFTdraw : IDisposable
    {
        private bool _continue = true;
        private SerialPort? _serialPort;
        private int _width = 10;
        private int _height = 10;
        private bool disposedValue;

        public FFTdraw() 
        {
            if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                throw new Exception("This is only supported on Windows");
            }
        }

        // Declare the delegate (if using non-generic pattern).
        public delegate void BitmapUpdateEventHandler(object sender, Bitmap bmp);

        // Declare the event.
        public event BitmapUpdateEventHandler? BitmapUpdateEvent;

        public void Initialize(string serialPort, int width, int height)
        {
            _width = width;
            _height = height;
            _serialPort = new SerialPort(serialPort);
            _serialPort.BaudRate = 115200;
            _serialPort.Open();
            Console.WriteLine(_serialPort.ReadLine());
            _serialPort.WriteLine("LOG p9");  // to start dumping FFT values
            Thread readThread = new Thread(ReadLoop);
        }

        public void ReadLoop()
        {
            while (_continue && _serialPort != null)
            {
                try
                {
                    List<float> list = _serialPort.ReadLine().Split(", ").Cast<float>().ToList();
                    if (list.Count != 1024)
                    {
                        Console.WriteLine("list.Count() = " + list.Count);
                    }

                    var factor = (_height - 40) / list.Max();
                    var bitmap = new Bitmap(_width, _height);
                    using (var g = Graphics.FromImage(bitmap))
                    {
                        var blackPen = new Pen(Color.Green, 1);
                        int x = 20;
                        foreach (var item in list)
                        {
                            g.DrawLine(blackPen, x, 20, x++, (int)Math.Round(item * factor));
                            if (x > _width) break;
                        }
                    }

                    BitmapUpdateEvent?.Invoke(this, bitmap);
                }
                catch (TimeoutException) { }
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            _continue = false;
            Thread.Sleep(1000);
            if (!disposedValue)
            {
                if (disposing && _serialPort != null)
                {
                    _serialPort.Close();
                    _serialPort = null;
                }

                // TODO: free unmanaged resources (unmanaged objects) and override finalizer
                // TODO: set large fields to null
                disposedValue = true;
            }
        }

        public void Dispose()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}