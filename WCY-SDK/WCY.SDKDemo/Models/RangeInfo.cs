using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WCY.SDKDemo.Models
{
    /// <summary>
    /// Rangeinfo
    /// </summary>
    public class RangeInfo
    {
        /// <summary>
        /// Range id
        /// </summary>
        public int RangeId { get; set; }
        /// <summary>
        /// K
        /// </summary>
        public double? DataK { get; set; }

        /// <summary>
        /// Range name
        /// </summary>
        public string RangeName { get; set; }
        /// <summary>
        /// unsigned
        /// </summary>
        public bool? IsSigned { get; set; }
        /// <summary>
        /// The data length of the current range
        /// </summary>
        public int? DataLength { get; set; }
        /// <summary>
        /// Unit of measurement
        /// </summary>
        public string Unit { get; set; }
        /// <summary>
        /// Reserved decimal places
        /// </summary>
        public double? DecimalCount { get; set; }
        /// <summary>
        /// Lower limit value
        /// </summary>
        public double? LowerLimit { get; set; }
        /// <summary>
        /// Upper limit value
        /// </summary>
        public double? UpperLimit { get; set; }
    }
}
