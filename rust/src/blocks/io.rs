mod csv {
  extern crate csv;
  mod read {
    use crate::block::Block;
    use crate::chainblocksc::CBStrings;
    use crate::chainblocksc::CBTypeInfo_Details;
    use crate::chainblocksc::CBTypeInfo_Details_Path;
    use crate::chainblocksc::CBType_ContextVar;
    use crate::chainblocksc::CBType_Path;
    use crate::chainblocksc::CBType_Seq;
    use crate::chainblocksc::CBType_String;
    use crate::chainblocksc::CBTypesInfo;
    use crate::core::getRootPath;
    use crate::core::init;
    use crate::core::referenceVariable;
    use crate::core::registerBlock;
    use crate::core::releaseVariable;
    use crate::types::common_type;
    use crate::types::ClonedVar;
    use crate::types::Context;
    use crate::types::ParameterInfo;
    use crate::types::Parameters;
    use crate::types::Type;
    use crate::types::Types;
    use crate::types::Var;
    use core::any::Any;
    use core::cell::Cell;
    use csv::Error;
    use csv::Reader;
    use csv::ReaderBuilder;
    use csv::StringRecord;
    use std::convert::TryFrom;
    use std::convert::TryInto;
    use std::ffi::CString;
    use std::io::Read;
    use std::path::Path;

    // either all in one go or one record per iteration
    // from string variable or path (a file)

    struct Row {
      strs: Vec<CString>,
      vars: Vec<Var>,
    }

    struct CSVRead {
      input_types: Types,
      strings: Types,
      output_types: Types,
      buffer_types: Types,
      parameters: Parameters,
      iterating: bool,
      looped: bool,
      reinit: bool,
      source: ClonedVar,
      records: Option<Box<dyn Iterator<Item = Result<StringRecord, Error>>>>,
      rows: Vec<Row>,
      slurp: Vec<Var>,
      headers: bool,
    }

    impl Default for CSVRead {
      fn default() -> Self {
        let path = Type {
          basicType: CBType_Path,
          details: CBTypeInfo_Details {
            path: CBTypeInfo_Details_Path {
              extensions: CBStrings::default(),
              existing: true,
              isFile: true,
              relative: true,
            },
          },
        };
        let buffer_types = vec![common_type::string, path];
        let buffer = Type {
          basicType: CBType_ContextVar,
          details: CBTypeInfo_Details {
            contextVarTypes: CBTypesInfo::from(&buffer_types),
          },
        };
        Self {
                    input_types: vec![common_type::none],
                    strings: vec![common_type::strings],
                    output_types: vec![common_type::strings],
                    buffer_types: buffer_types,
                    parameters: vec![
                        ParameterInfo::from((
                            "Source",
                            "A path or path variable to a file or a string buffer variable with CSV data.",
                            vec![path, buffer],
                        )),
                        ParameterInfo::from((
                            "Iterate",
                            "Reads the data one record at a time as in one record per chain iteration. Will output a empty sequence when done (if Looped is not selected).",
                            vec![common_type::bool],
                        )),
                        ParameterInfo::from((
                            "Looped",
                            "When Iterate is selected, if Looped is also selected, the iteration will restart from the first record rather than output a empty sequence.",
                            vec![common_type::bool],
                        )),
                            ParameterInfo::from((
                            "Headers",
                            "Assumes that first row is a headers row, skipping it.",
                            vec![common_type::bool],
                        )),
                    ],
                    iterating: false,
                    looped: false,
                    reinit: true,
                    source: ClonedVar(Var::default()),
                    records: None,
                    rows: Vec::<Row>::new(),
                    slurp: Vec::<Var>::new(),
                    headers: true,
                }
      }
    }

    impl CSVRead {
      fn load_file_reader(&mut self, path: &str) {
        let root_path = Path::new(getRootPath());
        let fullpath = root_path.join(path);
        let reader = ReaderBuilder::new()
          .has_headers(self.headers)
          .from_path(fullpath)
          .unwrap();
        self.records = Some(Box::new(reader.into_records()));
      }

      fn load_str_reader(&mut self, text: &'static [u8]) {
        let reader = ReaderBuilder::new()
          .has_headers(self.headers)
          .from_reader(text);
        self.records = Some(Box::new(reader.into_records()));
      }
    }

    impl Block for CSVRead {
      fn name(&mut self) -> &str {
        "CSV.Read"
      }
      fn inputTypes(&mut self) -> &Types {
        &self.input_types
      }
      fn outputTypes(&mut self) -> &Types {
        if self.iterating {
          self.output_types = vec![common_type::strings];
        } else {
          self.output_types = vec![Type {
            basicType: CBType_Seq,
            details: CBTypeInfo_Details {
              seqTypes: (&self.strings).into(),
            },
          }];
        }
        &self.output_types
      }
      fn parameters(&mut self) -> Option<&Parameters> {
        Some(&self.parameters)
      }
      fn setParam(&mut self, idx: i32, value: &Var) {
        match idx {
          0 => {
            self.source = value.into();
            self.reinit = true;
          }
          1 => {
            self.iterating = value.try_into().unwrap();
          }
          2 => {
            self.looped = value.try_into().unwrap();
          }
          3 => {
            self.headers = value.try_into().unwrap();
          }
          _ => {
            unimplemented!();
          }
        };
      }
      fn getParam(&mut self, idx: i32) -> Var {
        match idx {
          0 => self.source.0,
          1 => Var::from(self.iterating),
          2 => Var::from(self.looped),
          3 => Var::from(self.headers),
          _ => {
            unimplemented!();
          }
        }
      }
      fn activate(&mut self, context: &Context, _input: &Var) -> Var {
        if self.reinit {
          self.records = None;
          match self.source.0.valueType {
            CBType_Path => {
              let vcpath = CString::try_from(&self.source.0).unwrap();
              let vpath = vcpath.to_str().unwrap();
              self.load_file_reader(vpath);
            }
            CBType_ContextVar => {
              let var = referenceVariable(context, (&self.source.0).try_into().unwrap());
              match var.valueType {
                CBType_Path => {
                  let vcpath = CString::try_from(var).unwrap();
                  let vpath = vcpath.to_str().unwrap();
                  self.load_file_reader(vpath);
                }
                CBType_String => {
                  self.load_str_reader(var.try_into().unwrap());
                }
                _ => panic!("Wrong Source type (variable)"),
              }
              releaseVariable(var);
            }
            _ => panic!("Wrong Source type."),
          };
          self.reinit = false;
        }

        self.rows.clear();
        self.slurp.clear();
        if let Some(records) = self.records.as_mut() {
          if self.iterating {
            // a single seq of strings
            if let Some(record) = records.next() {
              if let Ok(data) = record {
                let it = data.iter();
                let mut row = Row {
                  strs: Vec::<CString>::new(),
                  vars: Vec::<Var>::new(),
                };
                for item in it {
                  let s = CString::new(item).unwrap();
                  row.vars.push(Var::from(&s));
                  row.strs.push(s);
                }
                self.rows.push(row);
                Var::from(&self.rows[0].vars)
              } else {
                Var::from(&self.slurp)
              }
            } else {
              if self.looped {
                // no optimal at all..
                // should use seek
                self.reinit = true;
              }
              Var::from(&self.slurp)
            }
          } else {
            // parses the whole file
            for record in records {
              if let Ok(data) = record {
                let it = data.iter();
                let mut row = Row {
                  strs: Vec::<CString>::new(),
                  vars: Vec::<Var>::new(),
                };
                for item in it {
                  let s = CString::new(item).unwrap();
                  row.vars.push(Var::from(&s));
                  row.strs.push(s);
                }
                self.rows.push(row);
              }
            }

            for row in &self.rows {
              self.slurp.push(Var::from(&row.vars));
            }

            Var::from(&self.slurp)
          }
        } else {
          Var::from(&self.slurp)
        }
      }
    }

    #[ctor]
    fn attach() {
      init();
      registerBlock::<CSVRead>("CSV.Read");
    }
  }

  mod write {
    // either append to file/buffer or overwrite each activation
  }
}
