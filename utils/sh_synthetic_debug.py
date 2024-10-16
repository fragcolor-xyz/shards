import lldb

# Constants for SHType enum values
SHType_None = 0
SHType_Any = 1
SHType_Enum = 2
SHType_Bool = 3
SHType_Int = 4
SHType_Int2 = 5
SHType_Int3 = 6
SHType_Int4 = 7
SHType_Int8 = 8
SHType_Int16 = 9
SHType_Float = 10
SHType_Float2 = 11
SHType_Float3 = 12
SHType_Float4 = 13
SHType_Color = 14
# ... other types ...
SHType_Bytes = 51
SHType_String = 52
SHType_Path = 53
SHType_ContextVar = 54
SHType_Image = 55
SHType_Seq = 56
SHType_Table = 57
SHType_Wire = 58
SHType_ShardRef = 59
SHType_Object = 60
SHType_Array = 61
SHType_Set = 62
SHType_Audio = 63
SHType_Type = 64
SHType_Trait = 65

# Mapping from SHType to human-readable names
SHType_Name_Map = {
    SHType_None: "None",
    SHType_Any: "Any",
    SHType_Enum: "Enum",
    SHType_Bool: "Bool",
    SHType_Int: "Int",
    SHType_Int2: "Int2",
    SHType_Int3: "Int3",
    SHType_Int4: "Int4",
    SHType_Int8: "Int8",
    SHType_Int16: "Int16",
    SHType_Float: "Float",
    SHType_Float2: "Float2",
    SHType_Float3: "Float3",
    SHType_Float4: "Float4",
    SHType_Color: "Color",
    SHType_Bytes: "Bytes",
    SHType_String: "String",
    SHType_Path: "Path",
    SHType_ContextVar: "ContextVar",
    SHType_Image: "Image",
    SHType_Seq: "Seq",
    SHType_Table: "Table",
    SHType_Wire: "Wire",
    SHType_ShardRef: "ShardRef",
    SHType_Object: "Object",
    SHType_Array: "Array",
    SHType_Set: "Set",
    SHType_Audio: "Audio",
    SHType_Type: "Type",
    SHType_Trait: "Trait",
}


class SHVarSyntheticProvider:
    """
    Synthetic provider for SHVar.
    Displays valueType and value based on valueType.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        try:
            # Cache valueType and payload
            value_type_sb = self.valobj.GetChildMemberWithName("valueType")
            if not value_type_sb.IsValid():
                self.valueType = None
                return

            self.valueType = value_type_sb.GetValueAsUnsigned()

            self.payload = self.valobj.GetChildMemberWithName("payload")

            # Map valueType to payload field name
            self.type_map = {
                SHType_Bool: "boolValue",
                SHType_Int: "intValue",
                SHType_Int2: "int2Value",
                SHType_Int3: "int3Value",
                SHType_Int4: "int4Value",
                SHType_Int8: "int8Value",
                SHType_Int16: "int16Value",
                SHType_Float: "floatValue",
                SHType_Float2: "float2Value",
                SHType_Float3: "float3Value",
                SHType_Float4: "float4Value",
                SHType_Color: "colorValue",
                SHType_Bytes: "bytesValue",
                SHType_String: "stringValue",
                SHType_Seq: "seqValue",
                SHType_Set: "setValue",
                SHType_Enum: "enumValue",  # Special handling
                # Add more types if needed
            }
        except Exception as e:
            pass

    def num_children(self):
        try:
            if self.valueType in self.type_map:
                return 2  # valueType and value
            else:
                return 1  # Only valueType
        except Exception as e:
            return 0

    def get_child_at_index(self, index):
        try:
            if index == 0:
                # Display valueType with human-readable name
                value_type = self.valobj.GetChildMemberWithName("valueType")
                type_name = SHType_Name_Map.get(
                    self.valueType, f"Unknown({self.valueType})"
                )
                summary = f"{self.valueType} ({type_name})"
                return value_type
            elif index == 1 and self.valueType in self.type_map:
                # Display the actual value based on valueType
                field_name = self.type_map[self.valueType]

                if self.valueType == SHType_String:
                    # Handle SHType_String
                    string_value = self.payload.GetChildMemberWithName("stringValue")
                    string_len = self.payload.GetChildMemberWithName(
                        "stringLen"
                    ).GetValueAsUnsigned()
                    string_cap = self.payload.GetChildMemberWithName(
                        "stringCapacity"
                    ).GetValueAsUnsigned()
                    return string_value
                elif self.valueType == SHType_Bytes:
                    # Handle SHType_Bytes
                    bytes_ptr = self.payload.GetChildMemberWithName("bytesValue")
                    bytes_size = self.payload.GetChildMemberWithName(
                        "bytesSize"
                    ).GetValueAsUnsigned()
                    bytes_cap = self.payload.GetChildMemberWithName(
                        "bytesCapacity"
                    ).GetValueAsUnsigned()
                    return bytes_ptr
                elif self.valueType == SHType_Enum:
                    # Handle SHType_Enum with vendorId and typeId
                    enum_val_obj = self.payload.GetChildMemberWithName("enumValue")
                    enum_vendor_id = self.payload.GetChildMemberWithName(
                        "enumVendorId"
                    ).GetValueAsSigned()
                    enum_type_id = self.payload.GetChildMemberWithName(
                        "enumTypeId"
                    ).GetValueAsSigned()
                    enum_value = enum_val_obj.GetValueAsUnsigned()
                    summary = f"Enum({enum_value}), VendorID={enum_vendor_id}, TypeID={enum_type_id}"
                    return enum_val_obj
                elif self.valueType == SHType_Color:
                    # Handle SHType_Color
                    color = self.payload.GetChildMemberWithName("colorValue")
                    r = color.GetChildMemberWithName("r").GetValueAsUnsigned()
                    g = color.GetChildMemberWithName("g").GetValueAsUnsigned()
                    b = color.GetChildMemberWithName("b").GetValueAsUnsigned()
                    a = color.GetChildMemberWithName("a").GetValueAsUnsigned()
                    summary = f"Color(r={r}, g={g}, b={b}, a={a})"
                    return color
                else:
                    # For other types, return the payload field
                    value = self.payload.GetChildMemberWithName(field_name)
                    return value
            elif index == 2:
                return self.payload
            else:
                return None
        except Exception as e:
            return None

    def get_child_index(self, name):
        try:
            if name == "valueType":
                return 0
            elif name in self.type_map.values():
                return 1
            elif name == "payload":
                return 2
            else:
                return -1
        except Exception as e:
            return -1


class SHSeqSyntheticProvider:
    """
    Synthetic provider for SHSeq.
    Displays the elements of the sequence.
    """

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        try:
            # Cache the elements pointer, len, and cap
            self.elements = self.valobj.GetChildMemberWithName("elements")
            self.len = self.valobj.GetChildMemberWithName("len").GetValueAsUnsigned()
            self.cap = self.valobj.GetChildMemberWithName("cap").GetValueAsUnsigned()
            # Get the type of elements (SHVar)
            self.element_type = self.elements.GetType().GetPointeeType()
            self.element_size = self.element_type.GetByteSize()
            # Get the address of elements
            self.elements_addr = self.elements.GetValueAsUnsigned()
        except Exception as e:
            pass

    def num_children(self):
        try:
            return self.len
        except Exception as e:
            return 0

    def get_child_at_index(self, index):
        try:
            if index >= self.len:
                return None

            # Calculate the address of the element
            element_addr = self.elements_addr + index * self.element_size

            target = self.valobj.GetTarget()
            sb_addr = lldb.SBAddress()
            sb_addr.SetLoadAddress(element_addr, target)

            # Create an SBValue for the element
            element_val = target.CreateValueFromAddress(
                f"[{index}]", sb_addr, self.element_type
            )
            if not element_val.IsValid():
                return None

            return element_val
        except Exception as e:
            return None

    def get_child_index(self, name):
        try:
            if name.startswith("[") and name.endswith("]"):
                index_str = name[1:-1]
                index = int(index_str)
                if 0 <= index < self.len:
                    return index
            return -1
        except Exception as e:
            return -1


def SHVarSummaryProvider(valobj, internal_dict):
    """
    Summary provider for SHVar.
    Provides a concise string representation based on valueType.
    """
    try:
        valobj = valobj.GetNonSyntheticValue()
        value_type_sb = valobj.GetChildMemberWithName("valueType")
        if not value_type_sb.IsValid():
            return "Invalid SHVar"

        valueType = value_type_sb.GetValueAsUnsigned()

        payload = valobj.GetChildMemberWithName("payload")
        if not payload.IsValid():
            return "Invalid SHVar"

        # Depending on the valueType, construct the summary
        if valueType == SHType_Bool:
            bool_val = payload.GetChildMemberWithName("boolValue").GetValue()
            summary = f"Bool: {bool_val}"
        elif valueType == SHType_Int:
            int_val = payload.GetChildMemberWithName("intValue").GetValue()
            summary = f"Int: {int_val}"
        elif valueType == SHType_Float:
            float_val = payload.GetChildMemberWithName("floatValue").GetValue()
            summary = f"Float: {float_val}"
        elif valueType == SHType_String:
            string_value = payload.GetChildMemberWithName("stringValue").GetSummary()
            # Remove the surrounding quotes from GetSummary()
            if string_value.startswith('"') and string_value.endswith('"'):
                string_value = string_value[1:-1]
            string_len = payload.GetChildMemberWithName(
                "stringLen"
            ).GetValueAsUnsigned()
            string_cap = payload.GetChildMemberWithName(
                "stringCapacity"
            ).GetValueAsUnsigned()
            summary = f'String: "{string_value}" (len={string_len}, cap={string_cap})'
        elif valueType == SHType_Seq:
            seq_val = payload.GetChildMemberWithName("seqValue")
            len_val = seq_val.GetChildMemberWithName("len").GetValueAsUnsigned()
            summary = f"Seq: {len_val} elements"
        elif valueType == SHType_Color:
            color = payload.GetChildMemberWithName("colorValue")
            r = color.GetChildMemberWithName("r").GetValueAsUnsigned()
            g = color.GetChildMemberWithName("g").GetValueAsUnsigned()
            b = color.GetChildMemberWithName("b").GetValueAsUnsigned()
            a = color.GetChildMemberWithName("a").GetValueAsUnsigned()
            summary = f"Color(r={r}, g={g}, b={b}, a={a})"
        elif valueType == SHType_Enum:
            enum_val_obj = payload.GetChildMemberWithName("enumValue")
            enum_vendor_id = payload.GetChildMemberWithName(
                "enumVendorId"
            ).GetValueAsSigned()
            enum_type_id = payload.GetChildMemberWithName(
                "enumTypeId"
            ).GetValueAsSigned()
            enum_value = enum_val_obj.GetValueAsUnsigned()
            summary = (
                f"Enum({enum_value}), VendorID={enum_vendor_id}, TypeID={enum_type_id}"
            )
        else:
            summary = f"SHVar (type: {SHType_Name_Map.get(valueType, 'Unknown')})"

        return summary
    except Exception as e:
        return "Error generating summary"


def SHSeqSummaryProvider(valobj, internal_dict):
    """
    Summary provider for SHSeq.
    Provides a concise string representation based on the sequence length.
    """
    try:
        valobj = valobj.GetNonSyntheticValue()
        len_val = valobj.GetChildMemberWithName("len").GetValueAsUnsigned()
        summary = f"Seq with {len_val} elements"
        return summary
    except Exception as e:
        return "Error generating summary"


def __lldb_init_module(debugger, internal_dict):
    """
    Initializes the LLDB module by registering synthetic and summary providers for SHVar and SHSeq.
    """
    try:
        # Register the synthetic providers
        debugger.HandleCommand(
            "type synthetic add -l sh_synthetic_debug.SHVarSyntheticProvider SHVar"
        )
        debugger.HandleCommand(
            "type synthetic add -l sh_synthetic_debug.SHSeqSyntheticProvider SHSeq"
        )

        # Register the summary providers as functions
        debugger.HandleCommand(
            "type summary add -F sh_synthetic_debug.SHVarSummaryProvider SHVar"
        )
        debugger.HandleCommand(
            "type summary add -F sh_synthetic_debug.SHSeqSummaryProvider SHSeq"
        )

        # Enable the synthetic provider and summary provider categories
        debugger.HandleCommand("type category enable sh_synthetic_debug")

    except Exception as e:
        pass
