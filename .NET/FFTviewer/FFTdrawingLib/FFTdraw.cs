using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
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
        private string _cmd;

        public FFTdraw(string serialPort, int width, int height, string cmd) 
        {
            if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                throw new Exception("This is only supported on Windows");
            }

            _width = width;
            _height = height;
            _cmd = cmd;
            _serialPort = new SerialPort(serialPort, 115200);
            _serialPort.ReadTimeout = 2000;
            _serialPort.ReadBufferSize = 10000000;
            _serialPort.Open();
            var thread = new Thread(ReadLoop);
            thread.Start();
        }

        // Declare the delegate (if using non-generic pattern).
        public delegate void BitmapUpdateEventHandler(object sender, Bitmap bmp);

        // Declare the event.
        public event BitmapUpdateEventHandler? BitmapUpdateEvent;

        public void ChangeSize(int width, int height, string cmd)
        {
            lock (this)
            {
                _width = width;
                _height = height;
                _cmd = cmd;
            }
        }

        public void ReadLoop()
        {
            Thread.Sleep(1000);
            _serialPort?.WriteLine(_cmd);  // to start dumping FFT values

            while (_continue && _serialPort != null)
            {
                try
                {
                    // _serialPort.WriteLine("hallo");
                    var text = _serialPort.ReadLine();
                    Debug.Print(text);
                    var list = text.Split(",", StringSplitOptions.RemoveEmptyEntries).Select(x => x.Trim());
                    if (list.Count() < 48)
                    {
                        Debug.Print("list.Count() = " + list.Count());
                    }
                    else
                    {
                        Debug.Print(String.Join(", ", list));
                        Debug.Print(list.Count().ToString());
                        var data = list.Select(x => (float)Int32.Parse(x, NumberStyles.HexNumber));
                        lock (this) // do not update the bitmap if the size is changing. 
                        {
                            BitmapUpdateEvent?.Invoke(this, DrawBitmap(data));
                        }
                    }
                }
                catch (Exception e) 
                { 
                    Debug.Print(e.Message); 
                }
            }
        }

        private Bitmap DrawBitmap(IEnumerable<float> data)
        {
            var factor = (_height - 40) / data.Skip(1).Max();  // remove .Skip(1)
            var bitmap = new Bitmap(_width, _height);
            using (var g = Graphics.FromImage(bitmap))
            {
                var penWidth = Math.Max(1, _width / data.Count()); // integer math. 
                var pen = new Pen(Color.Green, penWidth);
                int x = 20;
                foreach (var item in data)
                {
                    var barHeight = (int)Math.Round(item * factor);
                    g.DrawLine(pen, x, _height - 20, x, _height - 20 - barHeight);
                    x += penWidth;
                    if (x > _width) break;
                }
            }

            return bitmap;
        }

        protected virtual void Dispose(bool disposing)
        {
            _continue = false;
            Thread.Sleep(1500);
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