module Test where

import Prelude hiding (id,(.),(<*>),(<$>),pure,(*>),(<*),foldl,print)
import qualified Prelude as P

import OpenCog.Lojban
import OpenCog.Lojban.Util
import OpenCog.Lojban.WordList
import OpenCog.Lojban.Syntax
import OpenCog.Lojban.Syntax.Util
import OpenCog.Lojban.Syntax.Types
import OpenCog.Lojban.Syntax.AtomUtil

import OpenCog.AtomSpace

import Text.Syntax
import Text.Syntax.Parser.Naive
import Text.Syntax.Printer.Naive

import Control.Monad.Trans.Reader
import Control.Category (id,(.))

import Control.Isomorphism.Partial
import qualified Data.ListTrie.Patricia.Set.Ord as TS

import Data.Maybe
import qualified Data.Map as M

--import Text.XML.HXT.Core

init = do
    wl <- loadWordLists "lojban.xml"
    let mpa x y = rawparse (runReaderT x wl) y
    return (wl,mpa)
