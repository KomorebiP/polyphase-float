-- SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
-- SPDX-License-Identifier: LGPL-3.0-or-later
-- Keep a reference heading, purpose and prototype together where practicable.
function Code(inline)
  if FORMAT:match('latex') and inline.text:match('^[A-Za-z][A-Za-z0-9_]+$') and inline.text:find('_') then
    local escaped = inline.text:gsub('_', '\\_\\allowbreak{}')
    return pandoc.RawInline('latex', '\\texttt{' .. escaped .. '}')
  end
end

function Pandoc(document)
  if not FORMAT:match('latex') then return document end
  local result = pandoc.List()
  local function code_lines(index)
    for offset = 1, 3 do
      local following = document.blocks[index + offset]
      if following and following.t == 'CodeBlock' then
        local _, count = following.text:gsub('\n', '\n')
        return count + 1
      end
    end
    return 4
  end
  local function reference_need(index)
    return math.min(40, math.ceil(8 + code_lines(index) * 0.9))
  end
  local function table_need(block)
    local rows = {}
    for _, body in ipairs(block.bodies) do
      for _, row in ipairs(body.body) do table.insert(rows, row) end
    end
    if #rows > 10 then return nil end
    local columns = math.max(1, #block.colspecs)
    local lines = 3
    for _, row in ipairs(rows) do
      local height = 1
      for _, cell in ipairs(row.cells) do
        local text = pandoc.utils.stringify(cell.contents)
        height = math.max(height, math.ceil(#text / (70 / columns)))
      end
      lines = lines + height
    end
    return math.min(40, math.ceil(lines * 1.2 + 2))
  end
  local function reserve(lines)
    result:insert(pandoc.RawBlock('latex', '\\Needspace{' .. lines .. '\\baselineskip}'))
  end
  for index, block in ipairs(document.blocks) do
    if block.t == 'Header' and block.level == 2 then
      local following = document.blocks[index + 1]
      local required = 10
      if following and following.t == 'Header' and following.level == 3 then
        required = 4 + reference_need(index + 1)
      elseif following and following.t == 'Table' then
        required = 4 + (table_need(following) or 8)
      end
      reserve(required)
    elseif block.t == 'Header' and block.level == 3 and block.identifier:match('^ppf_') then
      reserve(reference_need(index))
    elseif block.t == 'CodeBlock' then
      local _, count = block.text:gsub('\n', '\n')
      if count < 20 then reserve(math.ceil((count + 1) * 0.9 + 2)) end
    elseif block.t == 'Table' then
      local required = table_need(block)
      if required then reserve(required) end
    end
    result:insert(block)
  end
  document.blocks = result
  return document
end
