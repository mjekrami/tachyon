use arrow::array::{Float64Builder, TimestampNanosecondBuilder};
use arrow::datatypes::{DataType, Field, Schema, SchemaRef, TimeUnit};
use arrow::record_batch::RecordBatch;
use datafusion::datasource::{TableProvider, TableType};
use datafusion::error::Result;
use datafusion::execution::context::SessionContext;
use datafusion::physical_plan::{
    DisplayAs, DisplayFormatType, ExecutionPlan, SendableRecordBatchStream,
};
use datafusion::prelude::*;
use futures::stream;
use std::any::Any;
use std::ffi::CString;
use std::sync::Arc;
use tokio_stream::Stream;

mod ffi;

#[derive(Debug)]
struct TachyonExec {
    symbol: String,
    schema: SchemaRef,
}

fn tachyon_stream(symbol: String, schema: SchemaRef) -> impl Stream<Item = Result<RecordBatch>> {
    const BATCH_SIZE: u32 = 4096;

    let mut ts_vec: Vec<u64> = vec![0; BATCH_SIZE as usize];
    let mut bid_vec: Vec<f64> = vec![0.0; BATCH_SIZE as usize];
    let mut ask_vec: Vec<f64> = vec![0.0; BATCH_SIZE as usize];

    let c_symbol = CString::new(symbol).unwrap();

    // SAFETY: call into C++ library.
    let scanner_handle = unsafe { ffi::tachyon_open_scanner(c_symbol.as_ptr()) };

    stream::unfold(scanner_handle, move |handle| {
        let mut c_batch = ffi::C_TickBatch {
            timestamps: ts_vec.as_mut_ptr(),
            bid_prices: bid_vec.as_mut_ptr(),
            ask_prices: ask_vec.as_mut_ptr(),
            num_ticks: 0,
        };

        let result = unsafe { ffi::tachyon_scanner_next_batch(handle, &mut c_batch, BATCH_SIZE) };

        if result == 0 {
            unsafe { ffi::tachyon_free_scanner(handle) };
            return None; // Signal the stream to end
        }

        // Convert the C data into an Arrow RecordBatch
        let num_ticks = c_batch.num_ticks as usize;
        let mut ts_builder = TimestampNanosecondBuilder::new();
        ts_builder.append_slice(&ts_vec[..num_ticks]);

        let mut bid_builder = Float64Builder::new();
        bid_builder.append_slice(&bid_vec[..num_ticks]);

        let mut ask_builder = Float64Builder::new();
        ask_builder.append_slice(&ask_vec[..num_ticks]);

        let record_batch = RecordBatch::try_new(
            schema.clone(),
            vec![
                Arc::new(ts_builder.finish()),
                Arc::new(bid_builder.finish()),
                Arc::new(ask_builder.finish()),
            ],
        )
        .unwrap();

        Some((Ok(record_batch), handle))
    })
}
impl ExecutionPlan for TachyonExec {
    fn as_any(&self) -> &dyn Any {
        self
    }
    fn schema(&self) -> SchemaRef {
        self.schema.clone()
    }
    fn output_partitioning(&self) -> datafusion::physical_plan::Partitioning {
        datafusion::physical_plan::Partitioning::UnknownPartitioning(1)
    }
    fn output_ordering(&self) -> Option<&[datafusion::physical_plan::PhysicalSortExpr]> {
        None
    }
    fn children(&self) -> Vec<Arc<dyn ExecutionPlan>> {
        vec![]
    }
    fn with_new_children(
        self: Arc<Self>,
        _: Vec<Arc<dyn ExecutionPlan>>,
    ) -> Result<Arc<dyn ExecutionPlan>> {
        Ok(self)
    }

    fn execute(
        &self,
        _partition: usize,
        _context: Arc<datafusion::execution::TaskContext>,
    ) -> Result<SendableRecordBatchStream> {
        let stream = tachyon_stream(self.symbol.clone(), self.schema.clone());
        Ok(Box::pin(
            datafusion::physical_plan::common::Streamadapter::new(self.schema(), stream),
        ))
    }
}

impl DisplayAs for TachyonExec {
    fn fmt(
        &self,
        f: &mut std::fmt::Formatter,
        _display_type: DisplayFormatType,
    ) -> std::fmt::Result {
        write!(f, "TachyonExec: symbol={}", self.symbol)
    }
}

#[derive(Clone)]
struct TachyonTableProvider {
    symbol: String,
    schema: SchemaRef,
}

impl TachyonTableProvider {
    fn new(symbol: &str) -> Self {
        let schema = Arc::new(Schema::new(vec![
            Field::new("ts", DataType::Timestamp(TimeUnit::Nanosecond, None), false),
            Field::new("bid", DataType::Float64, false),
            Field::new("ask", DataType::Float64, false),
        ]));
        Self {
            symbol: symbol.to_string(),
            schema,
        }
    }
}

#[async_trait::async_trait]
impl TableProvider for TachyonTableProvider {
    fn as_any(&self) -> &dyn Any {
        self
    }
    fn schema(&self) -> SchemaRef {
        self.schema.clone()
    }
    fn table_type(&self) -> TableType {
        TableType::Base
    }

    async fn scan(
        &self,
        _state: &dyn datafusion::logical_expr::TableProviderFilterPushDown,
        _projection: Option<&Vec<usize>>,
        _filters: &[datafusion::logical_expr::Expr],
        _limit: Option<usize>,
    ) -> Result<Arc<dyn ExecutionPlan>> {
        // Here you would implement predicate pushdown, passing filters down to C++
        let exec_plan = TachyonExec {
            symbol: self.symbol.clone(),
            schema: self.schema(),
        };
        Ok(Arc::new(exec_plan))
    }
}

// The Main Application
#[tokio::main]
async fn main() -> Result<()> {
    // Build the C++ library first!
    // `cd ../tachyon-tick/build && make`

    let ctx = SessionContext::new();

    let provider = TachyonTableProvider::new("AAPL");
    ctx.register_table("aapl_ticks", Arc::new(provider))?;

    let sql = "SELECT \
               ts, \
               ask - bid AS spread \
               FROM aapl_ticks \
               WHERE ask - bid > 0.019 \
               LIMIT 10";

    println!("Executing query:\n{}", sql);

    let df = ctx.sql(sql).await?;
    df.show().await?;

    Ok(())
}
